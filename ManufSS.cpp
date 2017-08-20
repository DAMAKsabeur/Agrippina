#include "ManufSS.h"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

using namespace netflix::device;
using namespace netflix;

uint8_t ManufSS::secStoreHmacKey[32] = { 0x4b, 0x40, 0xf4, 0x7e, 0xf6, 0x29, 0xd0, 0x1c,
                              0x1f, 0x37, 0xe8, 0x3d, 0x3d, 0x30, 0xcd, 0x28,
                              0x70, 0x22, 0xee, 0x40, 0x10, 0xf5, 0x81, 0x61,
                              0x64, 0x2a, 0xe1, 0x7e, 0xe1, 0x7e, 0xec, 0x19 };

uint8_t ManufSS::secStoreAesKey[16] = { 0xd8, 0x95, 0x7a, 0x33, 0x14, 0x4d, 0xe9, 0x30,
                             0x02, 0xcd, 0x32, 0x89, 0x6e, 0x57, 0xdf, 0xfc };
char ManufSS::readBuffer[nsMAX_SS_PROVISION_SIZE];
uint8_t ManufSS::fileBuffer[nsMAX_SS_PROVISION_SIZE];
const char ManufSS::cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char ManufSS::cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

/* help macros for serialization work */
#define MSS_PUT_WORD8(ADDR, VALUE) \
    do { *((uint8_t *)(ADDR)) = (VALUE) & 0xFF; } while(0)

#define MSS_PUT_WORD16(ADDR, VALUE) \
    do { *((uint8_t *)(ADDR)    ) = ((VALUE) & 0xFF00) >> 8; \
         *((uint8_t *)(ADDR) + 1) = ((VALUE) & 0x00FF)     ; } while(0)

#define MSS_PUT_WORD32(ADDR, VALUE) \
    do { *((uint8_t *)(ADDR)    ) = ((VALUE) & 0xFF000000) >> 24; \
         *((uint8_t *)(ADDR) + 1) = ((VALUE) & 0x00FF0000) >> 16; \
         *((uint8_t *)(ADDR) + 2) = ((VALUE) & 0x0000FF00) >>  8; \
         *((uint8_t *)(ADDR) + 3) = ((VALUE) & 0x000000FF)      ; } while(0)

#define MSS_PRODUCE_WORD8( p_, sz_, w8_ ) \
    do { \
        if ( (sz_) < sizeof(uint8_t) ) { \
            NTRACE(TRACE_TEE, "failed to write 8 bit word (%d bytes remaining)\n", (int)(sz_)); \
            goto serializeError; \
        } \
        \
        MSS_PUT_WORD8((p_), (w8_)); \
        \
        (p_) += sizeof(uint8_t); \
        (sz_) -= sizeof(uint8_t); \
    } while (0)

#define MSS_PRODUCE_WORD16( p_, sz_, w16_ ) \
    do { \
        if ( (sz_) < sizeof(uint16_t) ) { \
            NTRACE(TRACE_TEE, "failed to write 16 bit word (%d bytes remaining)\n", (int)(sz_)); \
            goto serializeError; \
        } \
        \
        MSS_PUT_WORD16((p_), (w16_)); \
        \
        (p_) += sizeof(uint16_t); \
        (sz_) -= sizeof(uint16_t); \
    } while (0)

#define MSS_PRODUCE_WORD32( p_, sz_, w32_ ) \
    do { \
        if ( (sz_) < sizeof(uint32_t) ) { \
            NTRACE(TRACE_TEE, "failed to write 32 bit word (%d bytes remaining)\n", (int)(sz_)); \
            goto serializeError; \
        } \
        \
        MSS_PUT_WORD32((p_), (w32_)); \
        \
        (p_) += sizeof(uint32_t); \
        (sz_) -= sizeof(uint32_t); \
    } while (0)

#define MSS_PRODUCE_BYTES( p_, sz_, n_, b_ ) \
    do { \
        if ( (sz_) < (n_) ) { \
            NTRACE(TRACE_TEE, "failed to write %d bytes (%d remaining)\n", (int)(n_), (int)(sz_)); \
            goto serializeError; \
        } \
        \
        memcpy( (p_), (b_), (n_) ); \
        \
        (p_) += (n_); \
        (sz_) -= (n_); \
    } while (0)

#define MSS_CONSUME_BYTES_PTR( p_, sz_, n_, pb_ ) \
    do { \
        if ( (n_) > (sz_) ) { \
            NTRACE(TRACE_TEE, "failed to read %d bytes (%d remaining)\n", (int)(n_), (int)(sz_)); \
            goto parseError; \
        } \
        \
        (pb_) = (p_); \
        \
        (p_) += (n_); \
        (sz_) -= (n_); \
    } while (0)

int ManufSS::createContent(const std::string &idFilename, const std::string &drmDir, DataBuffer &content)
{
    int         rc;
    int         bytesToWrite;

    uint8_t    *p = fileBuffer;
    uint32_t    remaining_bytes = sizeof fileBuffer;

    uint8_t    *totalLengthPtr;
    uint8_t    *hmacPtr;
    uint8_t    *padLengthPtr;

    uint8_t     ICV[SHA1_BLOCK_SIZE];
    uint16_t    padLength = 0;
    uint16_t    remainder;

    // clear output buffer
    content.clear();

    /* first, create secure store header */
    MSS_PRODUCE_WORD32( p, remaining_bytes, nfSS_FILE_MAGIC1 );
    MSS_PRODUCE_WORD32( p, remaining_bytes, nfSS_FILE_MAGIC2 );

    MSS_PRODUCE_WORD16( p, remaining_bytes, nfSS_FILE_VERSION );

    MSS_CONSUME_BYTES_PTR( p, remaining_bytes, sizeof(uint16_t), totalLengthPtr );

    MSS_CONSUME_BYTES_PTR( p, remaining_bytes, nfSS_HEADER_HMAC_SIZE, hmacPtr );

    MSS_CONSUME_BYTES_PTR( p, remaining_bytes, sizeof(uint16_t), padLengthPtr );

    /*
     * now, time to start building sections
     */

    // first section: management section
    if ( ( rc = addMgmtSection( &p, &remaining_bytes ) ) != 0 )
    {
        NTRACE(TRACE_TEE, "AddMgmtSection failed, bailing.\n" );
        goto ERROR;
    }

    // next section: NTBA read-only section (esn, kpe, kph)
    if ( ( rc = addNtbaSection( &p, &remaining_bytes, idFilename ) ) != 0 )
    {
        NTRACE(TRACE_TEE, "AddMgmtSection failed, bailing.\n" );
        goto ERROR;
    }

    // next section: DRM read-only section (model cert/privkey, device cert template)
    // add DRM section only if the required files are present
    if ((rc = checkDrmFiles(drmDir)) == 0)
    {
        if ( ( rc = addDrmSection( &p, &remaining_bytes, drmDir ) ) != 0 )
        {
            NTRACE(TRACE_TEE, "AddDrmSection failed, bailing.\n" );
            goto ERROR;
        }
    }
    else
    {
        NTRACE(TRACE_TEE, "WARNING! Not adding DRM section because required file(s) are missing\n" );
    }

    // Okay, no more sections, time to hash/encrypt

    // calculate pad length and apply pkcs7 padding if appropriate
    if ( ( remainder = ((p - padLengthPtr) + SHA1_BLOCK_SIZE) % CIPHER_BLOCK_SIZE ) != 0 )
        /* NOTE: this is *not* PKCS#7 padding! In PKCS#7 padding, the padding is a full block size when the content size is a multiple of the block size! */
        /* TODO: change to actual PKCS#7 padding. Better yet, reformat the whole Manufacturing Secure Store with best practices in mind. See NRDAPP-4143 */
    {
        padLength = CIPHER_BLOCK_SIZE - remainder;

        for ( int i = 1; i <= padLength; i++ )
            MSS_PRODUCE_WORD8( p, remaining_bytes, padLength );
    }

    /* set pad length at padLengthPtr */
    MSS_PUT_WORD16( padLengthPtr, padLength );

    /*
     * compute ICV over data (SHA1), add to buffer
     */
    rc = sha1( padLengthPtr, (p - padLengthPtr), ICV );
    if (rc != 0)
    {
        NTRACE(TRACE_TEE, "sha1 returned %d, exiting.\n", rc );
        return -1;
    }
    MSS_PRODUCE_BYTES( p, remaining_bytes, sizeof ICV, ICV );

    // section length
    MSS_PUT_WORD16( totalLengthPtr, p - padLengthPtr);

    /* encrypt data  */
    if ( ( rc = aesCbc128EncryptZeroIV( padLengthPtr, p - padLengthPtr, secStoreAesKey ) ) != 0 )
    {
        NTRACE(TRACE_TEE, "EncryptWithClientKey returned %d, exiting.\n", rc );
        return -1;
    }

    // compute ICV over secstore header
    rc = hmacSha256( fileBuffer, hmacPtr - fileBuffer, secStoreHmacKey, nfSS_HEADER_HMAC_SIZE, hmacPtr );
    if ( rc < 0 )
    {
        NTRACE(TRACE_TEE, "hmacSha256 returned %d, exiting.\n", rc );
        return -1;
    }

    // how many bytes?
    bytesToWrite = p - fileBuffer;
    p = fileBuffer;
    content.append<uint8_t>(p, bytesToWrite);

    return 0;

/* NOTREACHED */
ERROR:
serializeError:
parseError:
    return -1;
} //createSecureStore

int ManufSS::addMgmtSection( uint8_t **p_data, uint32_t *p_remaining_bytes )
{
    // no mercy
    if ((p_data == NULL) || (*p_data == NULL) || (p_remaining_bytes == NULL))
    {
        NTRACE(TRACE_TEE, "wrong input arguments\n");
        return -1;
    }

    uint8_t     *p = *p_data;
    uint32_t     remaining_bytes = *p_remaining_bytes;
    uint8_t     *sectionLengthPtr;
    uint16_t     sectionLength = 0;

    uint8_t      NtbaROClientId[] = nfSS_CLIENT_ID_TEE_NTBA_RO;
    uint8_t      NtbaRWClientId[] = nfSS_CLIENT_ID_TEE_NTBA_RW;
    uint8_t      DrmROClientId[] = nfSS_CLIENT_ID_TEE_DRM_RO;
    uint8_t      DrmRWClientId[] = nfSS_CLIENT_ID_TEE_DRM_RW;
    uint8_t      NrdClientId[] = nfSS_CLIENT_ID_NRD_APP;

    /* Here's what we need to add:
     *
     * TEE MGMT Section ID                 4 bytes
     * section length                      2 bytes
     * NTBA RO section ID                  4 bytes
     * NTBA RO client ID                  16 bytes
     * NTBA RW section ID                  4 bytes
     * NTBA RW client ID                  16 bytes
     * DRM RO section ID                   4 bytes
     * DRM RO client ID                   16 bytes
     * DRM RW section ID                   4 bytes
     * DRM RW client ID                   16 bytes
     * NRD section ID                      4 bytes
     * NRD client ID                      16 bytes
     */

    MSS_PRODUCE_WORD32( p, remaining_bytes, teeSID_TEE_MGMT );

    // save section length pointer, we'll fill this in shortly
    MSS_CONSUME_BYTES_PTR( p, remaining_bytes, sizeof(uint16_t), sectionLengthPtr );

    MSS_PRODUCE_WORD32( p, remaining_bytes, teeSID_NTBA_RO );
    MSS_PRODUCE_BYTES( p, remaining_bytes, sizeof NtbaROClientId, NtbaROClientId );

    MSS_PRODUCE_WORD32( p, remaining_bytes, teeSID_NTBA_RW );
    MSS_PRODUCE_BYTES( p, remaining_bytes, sizeof NtbaRWClientId, NtbaRWClientId );

    MSS_PRODUCE_WORD32( p, remaining_bytes, teeSID_DRM_RO );
    MSS_PRODUCE_BYTES( p, remaining_bytes, sizeof DrmROClientId, DrmROClientId );

    MSS_PRODUCE_WORD32( p, remaining_bytes, teeSID_DRM_RW );
    MSS_PRODUCE_BYTES( p, remaining_bytes, sizeof DrmRWClientId, DrmRWClientId );

    MSS_PRODUCE_WORD32( p, remaining_bytes, teeSID_NRD_APP1 );
    MSS_PRODUCE_BYTES( p, remaining_bytes, sizeof NrdClientId, NrdClientId );

    // update sectionLength
    sectionLength = (p - sectionLengthPtr) - sizeof sectionLength;
    MSS_PUT_WORD16( sectionLengthPtr, sectionLength );

    *p_data = p;
    *p_remaining_bytes = remaining_bytes;

    return 0;

/* NOT REACHED */
serializeError:
parseError:
    return -1;
}/* end AddMgmtSection */

int ManufSS::processEsn(FILE *ifp, uint8_t **p_data, uint32_t *p_remaining_bytes) {
    int rc = 0;
    uint16_t esnLength = 0;
    uint8_t *p = *p_data;
    uint32_t remaining_bytes = *p_remaining_bytes;

    if ( fgets(readBuffer, sizeof(readBuffer), ifp) == NULL ) {
        // error or EOF, give message and bail
        NTRACE(TRACE_TEE, "failed to read esn from idfile, aborting.\n" );
        rc = -1;
        goto exit;
    }

    // remove newline, if present
    trimNewLine(readBuffer);

    // esn should not be more than ESN_MAX characters
    if ( strlen(readBuffer) > ESN_MAX ) {
        NTRACE(TRACE_TEE, "esn too long (%zu/%d) - exiting.\n", strlen(readBuffer), ESN_MAX );
        rc = -1;
        goto exit;
    }

    // XXX - should we do esn validation here???

    // add esn key, length, and esn
    MSS_PRODUCE_WORD32( p, remaining_bytes, ntbaAGENT_KEY_ESN );

    esnLength = strlen(readBuffer);
    MSS_PRODUCE_WORD16( p, remaining_bytes, esnLength );

    MSS_PRODUCE_BYTES( p, remaining_bytes, esnLength, readBuffer );

    *p_data = p;
    *p_remaining_bytes = remaining_bytes;
exit:
    return rc;

serializeError:
    return -1;
}


int ManufSS::processKpe(FILE *ifp, uint8_t **p_data, uint32_t *p_remaining_bytes) {
    int rc = 0;
#define B64_KPE_MAX 24
    char kpe[CIPHER_BLOCK_SIZE] = { 0 };
    uint8_t *p = *p_data;
    uint32_t remaining_bytes = *p_remaining_bytes;

    if ( fgets(readBuffer, sizeof(readBuffer), ifp) == NULL ) {
        // error or EOF, give message and bail
        NTRACE(TRACE_TEE, "failed to read Kpe, exiting.\n" );
        rc = -1;
        goto exit;
    }

    // remove newline, if present
    trimNewLine(readBuffer);

    // B64 kpe should not be more than B64_KPE_MAX characters
    if ( strlen(readBuffer) > B64_KPE_MAX ) {
        NTRACE(TRACE_TEE, "Kpe too long (%zu/%d) - exiting.\n", strlen(readBuffer), B64_KPE_MAX );
        rc = -1;
        goto exit;
    }

    // decode Kpe
    if ( b64_decode( readBuffer, kpe ) != 0 ) {
        NTRACE(TRACE_TEE, "Kpe decode failed, exiting.\n" );
        rc = -1;
        goto exit;
    }

    // add kpe key, length, and kpe
    MSS_PRODUCE_WORD32( p, remaining_bytes, ntbaAGENT_KEY_KPE );

    MSS_PRODUCE_WORD16( p, remaining_bytes, sizeof kpe );

    MSS_PRODUCE_BYTES( p, remaining_bytes, sizeof kpe, kpe );

    *p_data = p;
    *p_remaining_bytes = remaining_bytes;
exit:
    return rc;

serializeError:
    return -1;
}

int ManufSS::processKph(FILE *ifp, uint8_t **p_data, uint32_t *p_remaining_bytes) {
    int rc = 0;
#define B64_KPH_MAX 48
    char kph[HMAC_BLOCK_SIZE] = { 0 };
    uint8_t *p = *p_data;
    uint32_t remaining_bytes = *p_remaining_bytes;

    if ( fgets(readBuffer, sizeof(readBuffer), ifp) == NULL ) {
        // error or EOF, give message and bail
        NTRACE(TRACE_TEE, "failed to read Kph, exiting.\n" );
        rc = -1;
        goto exit;
    }

    // remove newline, if present
    trimNewLine(readBuffer);

    // B64 kph should not be more than B64_KPH_MAX characters
    if ( strlen(readBuffer) > B64_KPH_MAX ) {
        NTRACE(TRACE_TEE, "Kph too long (%zu/%d) - exiting.\n", strlen(readBuffer), B64_KPH_MAX );
        rc = -1;
        goto exit;
    }

    // save decoded kph
    if ( b64_decode( readBuffer, kph ) != 0 ) {
        NTRACE(TRACE_TEE, "Kph decode failed, exiting.\n" );
        rc = -1;
        goto exit;
    }

    // add kph key, length, and kph
    MSS_PRODUCE_WORD32( p, remaining_bytes, ntbaAGENT_KEY_KPH );

    MSS_PRODUCE_WORD16( p, remaining_bytes, sizeof kph );

    MSS_PRODUCE_BYTES( p, remaining_bytes, sizeof kph, kph );

    *p_data = p;
    *p_remaining_bytes = remaining_bytes;
exit:
    return rc;

serializeError:
    return -1;
}

int ManufSS::addNtbaSection( uint8_t **p_data, uint32_t *p_remaining_bytes, const std::string &idFilename )
{
    // no mercy
    if ((p_data == NULL) || (*p_data == NULL)
          || (p_remaining_bytes == NULL)
          || (idFilename.empty()))
    {
        NTRACE(TRACE_TEE, "wrong input arguments\n");
        return -1;
    }

    int         rc = 0;
    FILE       *ifp = NULL;
    uint8_t    *p = *p_data;
    uint32_t    remaining_bytes = *p_remaining_bytes;
    uint8_t    *sectionLengthPtr;
    uint16_t    sectionLength = 0;


    /*
     * Here's what we need to do:
     *
     *  - construct idfile path/filename
     *     - open idfile
     *  - read in esn, kpe, kph
     *  - ADD NTBA-RO section to *p_data
     */

    // open input file
    if ( ( ifp = fopen(idFilename.c_str(), "r")) == NULL ) {
        NTRACE(TRACE_TEE, "failed to open idfile for input, aborting.\n");
        rc = -1;
        goto exit;
    }

    /*
     * add section to buffer...
     */

    // add section ID
    MSS_PRODUCE_WORD32( p, remaining_bytes, teeSID_NTBA_RO );

    // save section length pointer, we'll fill this in shortly
    MSS_CONSUME_BYTES_PTR( p, remaining_bytes, sizeof(uint16_t), sectionLengthPtr );


    rc = processEsn(ifp, &p, &remaining_bytes);
    if (0 != rc)  goto exit;

    rc = processKpe(ifp, &p, &remaining_bytes);
    if (0 != rc) goto exit;

    rc = processKph(ifp, &p, &remaining_bytes);
    if (0 != rc) goto exit;

    // update sectionLength
    sectionLength = p - sectionLengthPtr - sizeof sectionLength;
    MSS_PUT_WORD16( sectionLengthPtr, sectionLength );

    *p_data = p;
    *p_remaining_bytes = remaining_bytes;
 exit:
    if (NULL != ifp) fclose(ifp);
    return rc;

/* NOT REACHED */
serializeError:
parseError:
    rc = -1;
    goto exit;
}/* end AddNtbaSection */

int ManufSS::addDrmSectionItem( uint8_t **p_data, uint32_t *p_remaining_bytes, const std::string &drmDir, const char *fileName, drm_RO_AgentClientKeys keyId ) {

    int         rc = 0;
    uint8_t    *p = *p_data;
    uint32_t    remaining_bytes = *p_remaining_bytes;

    std::string filePath;
    FILE       *ifp = NULL;
    struct stat statBuf;
    size_t      dataSz;
    uint8_t    *dataPtr;
    size_t      bytesRead;

    // open input file
    filePath = drmDir + fileName;
    if ( ( ifp = fopen(filePath.c_str(), "r")) == NULL )
    {
        NTRACE(TRACE_TEE, "failed to open %s for input, aborting.\n", fileName );
        goto ERROR;
    }

    if ( stat( filePath.c_str(), &statBuf ) != 0 )
    {
        NTRACE(TRACE_TEE, "stat() failed for %s, aborting.\n", fileName );
        goto ERROR;
    }
    dataSz = statBuf.st_size;
    if ( dataSz >= 65536 )
    {
        NTRACE(TRACE_TEE, "statbuf size is greater than  65536, aborting.\n");
        goto ERROR;
    }

    // add key, length, and PlayReady model private key
    MSS_PRODUCE_WORD32( p, remaining_bytes, keyId );

    MSS_PRODUCE_WORD16( p, remaining_bytes, dataSz );

    // read into memory
    MSS_CONSUME_BYTES_PTR( p, remaining_bytes, dataSz, dataPtr );
    bytesRead = fread( dataPtr, 1, dataSz, ifp );
    if ( bytesRead < dataSz )
    {
        NTRACE(TRACE_TEE, "fread(%s) returned %zu, aborting.\n", fileName, bytesRead );
        goto ERROR;
    }

    *p_data = p;
    *p_remaining_bytes = remaining_bytes;
exit:
    if (NULL != ifp) {
        (void) fclose(ifp);
    }
    return rc;

/* NOTREACHED */
ERROR:
parseError:
serializeError:
    rc = -1;
    goto exit;
}

int ManufSS::addDrmSection( uint8_t **p_data, uint32_t *p_remaining_bytes, const std::string &drmDir)
{
    // no mercy
    if ((p_data == NULL) || (*p_data == NULL)
          || (p_remaining_bytes == NULL)
          || (drmDir.empty()))
    {
        NTRACE(TRACE_TEE, "wrong input arguments\n");
        return -1;
    }

    int         rc = 0;
    uint8_t    *p = *p_data;
    uint32_t    remaining_bytes = *p_remaining_bytes;
    uint8_t    *sectionLengthPtr;
    uint16_t    sectionLength = 0;

    /*
     * Here's what we need to do:
     *
     *    - add DRM-RO section header
     *  - for file in {devcerttemplate.dat,groupcert.dat,grouppriv.dat} do
     *      - construct path/filename
     *      - get filesize, validate against remaining space
     *         - open file
     *      - add key, length
     *        - read in data from file
     *        - close file
     *  - update total length
     *  - update *p_data
     */

    /*
     * add section to buffer...
     */
    MSS_PRODUCE_WORD32( p, remaining_bytes, teeSID_DRM_RO );

    // save section length pointer, we'll fill this in shortly
    MSS_CONSUME_BYTES_PTR( p, remaining_bytes, sizeof(uint16_t), sectionLengthPtr );

    // Model Private Key
    rc = addDrmSectionItem( &p, &remaining_bytes, drmDir, nfSS_DRM_GROUPPRIVKEY_FILENAME, drmAGENT_KEY_MODEL_PRIV );
    if ( rc != 0 ) {
        NTRACE(TRACE_TEE, "failed to add DRM item for >%s<\n", nfSS_DRM_GROUPPRIVKEY_FILENAME);
        goto exit;
    }

    // Model Certificate
    rc = addDrmSectionItem( &p, &remaining_bytes, drmDir, nfSS_DRM_GROUPCERT_FILENAME, drmAGENT_KEY_MODEL_CERT );
    if ( rc != 0 ) {
        NTRACE(TRACE_TEE, "failed to add DRM item for >%s<\n", nfSS_DRM_GROUPCERT_FILENAME);
        goto exit;
    }

#ifndef PLAYREADY3
    // Device Certificate Template
    rc = addDrmSectionItem( &p, &remaining_bytes, drmDir, nfSS_DRM_DEVCERT_TEMPLATE_FILENAME, drmAGENT_KEY_DEVCERT_TEMPLATE );
    if ( rc != 0 ) {
        NTRACE(TRACE_TEE, "failed to add DRM item for >%s<\n", nfSS_DRM_DEVCERT_TEMPLATE_FILENAME);
        goto exit;
    }
#endif

    // update sectionLength
    sectionLength = p - sectionLengthPtr - sizeof sectionLength;
    MSS_PUT_WORD16( sectionLengthPtr, sectionLength );

    *p_data = p;
    *p_remaining_bytes = remaining_bytes;
exit:
    return rc;

/* NOTREACHED */
serializeError:
parseError:
    rc = -1;
    goto exit;

 }/* end AddDrmSection */

int ManufSS::aesCbc128EncryptZeroIV( uint8_t     *cleartextPtr,
                                     uint32_t    cleartextLength,
                                     uint8_t     *aesKeyPtr )
{
    // no mercy
    if ((cleartextPtr == NULL) || (cleartextLength <= 0)
          || (aesKeyPtr == NULL))
    {
        NTRACE(TRACE_TEE, "wrong input arguments\n");
        return -1;
    }

    if ( cleartextLength % 16 != 0 )
    {
        NTRACE(TRACE_TEE, "%d-byte buffer needs %d bytes of padding.\n",
                 cleartextLength, cleartextLength % 16 );
        return -1;
    }

    uint8_t *temp_data;
    if ( ( temp_data = (uint8_t *) malloc( cleartextLength ) ) == NULL )
    {
        NTRACE(TRACE_TEE, "can't allocated %d bytes.\n",
                cleartextLength );
        return -1;
    }

    AES_KEY localKey;
    (void) bzero(&localKey, sizeof(AES_KEY) );
    if ( AES_set_encrypt_key(aesKeyPtr, 128, &localKey) < 0 )
    {
        NTRACE(TRACE_TEE, "AES_set_encrypt_key() failed\n");
        free(temp_data);
        return -1;
    }

    uint8_t iv[16];
    (void) bzero(iv, sizeof(iv) );
    AES_cbc_encrypt( cleartextPtr, temp_data, cleartextLength, &localKey, iv, AES_ENCRYPT);

    // now, copy encrypted data over top of cleartext
    (void) memcpy( cleartextPtr, temp_data, cleartextLength );

    // free malloc'd mem...
    (void) free(temp_data);
    return 0;
}/* end aesCbc128EncryptZeroIV */

int ManufSS::sha1( uint8_t     *dataPtr,
                   uint32_t    dataLength,
                   uint8_t     *resultPtr )
{
    // no mercy
    if ((dataPtr == NULL) || (dataLength <= 0)
          || (resultPtr == NULL))
    {
        NTRACE(TRACE_TEE, "wrong input arguments\n");
        return -1;
    }

    //initialize hash function
    SHA_CTX    sha1Context;
    SHA1_Init(&sha1Context);

    // add the input buffer for hash creation
    SHA1_Update(&sha1Context, dataPtr, dataLength );

    // Finalize and save the hash data
    SHA1_Final(resultPtr, &sha1Context);

    return 0;
}/* end sha1 */

int ManufSS::hmacSha256( uint8_t     *dataPtr,
                         uint32_t    dataLength,
                         uint8_t     *keyPtr,
                         uint32_t    keyLength,
                         uint8_t     *resultPtr )
{
    // no mercy
    if ((dataPtr == NULL) || (dataLength <= 0)
          || (keyPtr == NULL) || (keyLength <= 0)
          || (resultPtr == NULL))
    {
        NTRACE(TRACE_TEE, "wrong input arguments\n");
        return -1;
    }

    HMAC_CTX    context;
    uint32_t    length = 0;
    HMAC_Init(&context, keyPtr, keyLength, EVP_sha256(  ));
    HMAC_Update(&context, dataPtr, dataLength);
    HMAC_Final(&context, resultPtr, &length);
    // fixes NRDP-3094
    HMAC_CTX_cleanup(&context);

    return length > 0 ? length : -1;
}/* end hmacSha256 */

void ManufSS::decodeblock( unsigned char *in, unsigned char *out )
{
    out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
    out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
    out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

int ManufSS::b64_decode( char *inPtr, char *outPtr )
{
    int retcode = 0;
    unsigned char in[4];
    unsigned char out[3];
    int v;
    int i, len;

    *in = (unsigned char) 0;
    *out = (unsigned char) 0;
    while( *inPtr != '\0' )
    {
        for( len = 0, i = 0; i < 4 && *inPtr != '\0'; i++ ) {
            v = 0;
            while ( *inPtr && v == 0 )
            {
                v = *inPtr++;
                v = ((v < 43 || v > 122) ? 0 : (int) cd64[ v - 43 ]);
                if( v != 0 )
                {
                    v = ((v == (int)'$') ? 0 : v - 61);
                }
            }/* end while ( *inPtr && v == 0 ) */
            if ( *inPtr )
            {
                len++;
                if( v != 0 )
                {
                    in[ i ] = (unsigned char) (v - 1);
                }
            }
            else
            {
                in[i] = (unsigned char) 0;
            }
        }
        if ( len > 0 )
        {
            decodeblock( in, out );
            for( i = 0; i < len - 1; i++ )
            {
                *outPtr++ = out[i];
            }
        }/* end if ( len > 0 ) */

    }/* end while( *inPtr != '\0' ) */
    return( retcode );
}

void ManufSS::trimNewLine(char *str)
{
    int len = strlen(str);

    if (str[len - 1] == '\r')    // ends in \r
        str[len - 1] = '\0';
    else if ((str[len - 1] == '\n') && (str[len - 2] == '\r')) // ends in /r/n
        str[len - 2] = '\0';
    else if (str[len - 1] == '\n') // ends in \n
        str[len - 1] = '\0';
}

int ManufSS::checkDrmFiles(const std::string &drmDir)
{
    std::string filePath;
    FILE *file;

    filePath = drmDir + nfSS_DRM_GROUPPRIVKEY_FILENAME;
    if ((file = fopen(filePath.c_str(), "r"))) {
        fclose(file);
    } else {
        NTRACE(TRACE_TEE, "WARNING! %s is missing\n", nfSS_DRM_GROUPPRIVKEY_FILENAME);
        return -1;
    }

    filePath = drmDir + nfSS_DRM_GROUPCERT_FILENAME;
    if ((file = fopen(filePath.c_str(), "r"))) {
        fclose(file);
    } else {
        NTRACE(TRACE_TEE, "WARNING! %s is missing\n", nfSS_DRM_GROUPCERT_FILENAME);
        return -1;
    }

    filePath = drmDir + nfSS_DRM_DEVCERT_TEMPLATE_FILENAME;
    if ((file = fopen(filePath.c_str(), "r"))) {
        fclose(file);
    } else {
        NTRACE(TRACE_TEE, "WARNING! %s is missing\n", nfSS_DRM_DEVCERT_TEMPLATE_FILENAME);
        return -1;
    }

    return 0;
}
