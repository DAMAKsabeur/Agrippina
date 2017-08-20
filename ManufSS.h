/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef MANUFSS_H_
#define MANUFSS_H_

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <nrd/AppLog.h>

namespace netflix {
namespace device {

#define nfSS_DRM_GROUPCERT_FILENAME	"bgroupcert.dat"
#define nfSS_DRM_GROUPPRIVKEY_FILENAME  "zgpriv.dat"
#define nfSS_DRM_DEVCERT_TEMPLATE_FILENAME	"devcerttemplate.dat"

#define nfSS_MGMT_SECTION_SIZE  106

// the following definitions depend on this file version
#define nfSS_FILE_VERSION_SIZE  2
#define nfSS_FILE_VERSION       0x0001

// total length field size
#define nfSS_FILE_LENGTH_SIZE   2 /* sizeof(uint16_t) */

// file magic for secure store file (first two words of file)
#define nfSS_FILE_MAGIC_SIZE    8
#define nfSS_FILE_MAGIC1        0xF00DFEED
#define nfSS_FILE_MAGIC2        0xFADEFEED

// each client gets its own section, which has its own magic number
#define nfSS_SECTION_MAGIC  0xFEEDBEEF

#define nfSS_FILE_HEADER_SIZE  (nfSS_FILE_MAGIC_SIZE + nfSS_FILE_VERSION_SIZE + nfSS_FILE_LENGTH_SIZE)
#define nfSS_HEADER_HMAC_SIZE   32  // HMAC_SHA256 is 32 bytes

#define nfSS_RECORD_ICV_SIZE   20 // we are currently using SHA1 as the ICV

/*
 * Following are the currently defined ("authorized") secure store clients
 * that live in the TEE. Note that for NTBA, we define two client IDs; this
 * allows segregation of the read-only elements, so that we don't have to
 * recompute the integrity hash over those every time the read-write stuff
 * (e.g. Kce/Kch) changes.
 */
#define nfSS_CLIENT_ID_SIZE     16 /* client ID is 16 bytes */

// NB: special client ID for tee mgmt (all 0x00)
#define nfSS_CLIENT_ID_TEE_MGMT   {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}

#define nfSS_CLIENT_ID_TEE_NTBA_RO   {0x9d,0x4a,0x88,0xc4,0x32,0x62,0x78,0x7c,0x8f,0x27,0xba,0xed,0xa8,0x71,0x61,0x85}
#define nfSS_CLIENT_ID_TEE_NTBA_RW   {0x72,0x90,0xae,0x07,0xbd,0x8d,0x07,0x74,0xac,0x6a,0xde,0x07,0xd5,0xa7,0xce,0xc9}

// DRM agent has two different sections, one for model key(s) and one for device keys
#define nfSS_CLIENT_ID_TEE_DRM_RO  {0xb9,0x0e,0x81,0x64,0x91,0x67,0x59,0xaa,0xa8,0x63,0x1d,0x0d,0xc9,0xac,0xb8,0x66}
#define nfSS_CLIENT_ID_TEE_DRM_RW  {0xc3,0x1e,0x29,0x98,0x11,0x00,0x6c,0x5a,0xff,0x1f,0xdd,0x73,0x29,0x90,0x0b,0x01}

// NRD app has one client ID
#define nfSS_CLIENT_ID_NRD_APP  {0x15,0xe1,0x25,0xe9,0x11,0x00,0xF1,0x99,0x03,0x33,0xd1,0xfd,0x90,0x87,0xbb,0x40}

/*
 * The section IDs are visible (unencrypted) in the secure store; only the
 * secstore mgmt code can map the section IDs to client IDs
 */
typedef enum teeSectionID
{
    // add section IDs for NRD app here...
    teeSID_NRD_APP1 = 0x0000EE00,

    // add section IDs for TEE agents below...
    teeSID_TEE_MGMT = 0xF00FEE00, // tee agent section IDs must be >= this
    teeSID_NTBA_RO = 0xF00FEE01,
    teeSID_NTBA_RW = 0xF00FEE02,
    teeSID_DRM_RO = 0xF00FEE03,
    teeSID_DRM_RW = 0xF00FEE04,
} teeSectionIDType;

/*
 * Currently defined secure store clients that live in the rich OS; note that
 * your rich OS application must know this identifier in order to access
 * its secure store. It will use this identifier only when establishing
 * a TEE client API "session" with the secure store agent.
 */
#define nfSS_CLIENT_ID_RICHOS_NRD       {0x7e,0xef,0x55,0x84,0x30,0xb3,0xa3,0xc3,0xd2,0xe5,0x50,0x80,0x89,0xde,0x9d,0xef}

// write-protect flag -- normal insert/delete routines cannot modify these records
#define nfSS_WRITE_PROTECT_FLAG 0x80000000

#define nfSS_IS_WRITE_PROTECTED(x) (((x) & nfSS_WRITE_PROTECT_FLAG) != 0)

#ifdef PLAYREADY3
// FIXME: +4K bytes for now, must figure out the right size
#define nsMAX_SS_PROVISION_SIZE (44+2+108+126+5628+35+4096)
#else
#define nsMAX_SS_PROVISION_SIZE (44+2+108+126+5628+35)
#endif
#define nsMAX_SS_NTBA_RO_SIZE 126
#define nsMAX_SS_DRM_RO_SIZE 5638

// sizes of ESN, keys
#define ESN_MAX             42
#define CIPHER_BLOCK_SIZE   16
#define HMAC_BLOCK_SIZE     32
#define SHA1_BLOCK_SIZE     20

/*
 * NOTE: client keys must be unique within that client's domain. If you
 * add more NTBA keys, make sure they are different than the ones defined
 * below. Using an enum is a good way to ensure that this is true.
 *
 * Here are the keys used by the NTBA agent for ESN, Kpe, Kph:
 */
enum ntba_RW_AgentClientKeys
{
    // these elements are dynamically updated
    ntbaAGENT_KEY_KCE = 0x00000001,
    ntbaAGENT_KEY_KCH = 0x00000002,
};

enum ntba_RO_AgentClientKeys
{
    // these elements are write-protected
    ntbaAGENT_KEY_ESN = 0x80000001,
    ntbaAGENT_KEY_KPE = 0x80000002,
    ntbaAGENT_KEY_KPH = 0x80000003,
};

enum drm_RO_AgentClientKeys
{
    // these elements are write-protected
    drmAGENT_KEY_MODEL_PRIV = 0x80010001,
    drmAGENT_KEY_MODEL_CERT = 0x80010002,
#ifndef PLAYREADY3
    drmAGENT_KEY_DEVCERT_TEMPLATE = 0x80010003,
#endif
};

class ManufSS
{
public:
    /*********************************************************************
	 * Function: createContent
	 *
	 * Purpose: creates the contents for manufacturing securestore
	 *
	 * Inputs: idFilename    -   Idfile name/path
	 * 	       drmDir        -   DRM files directory
	 *         (out) content -   manufacturing secure store content
	 * returns: 0 if everything okay
	 *          -1 otherwise
	 *
	 *********************************************************************/
	static int createContent(const std::string &idFilename, const std::string &drmDir, DataBuffer &content);

private:
	/*********************************************************************
	 * Function: addMgmtSection
	 *
	 * Purpose: add management section to secure store file
	 *
	 * Inputs: bufPtrPtr - pointer to pointer to buffer
	 *         maxLength - bytes available in *BufPtrPtr
	 *
	 * Outputs: Updates stores data at *BufPtrPtr, updates *BufPtrPtr
	 *
	 * returns: 0 if everything okay
	 *          -1 otherwise
	 *
	 *********************************************************************/
	static int addMgmtSection( uint8_t **p_data, uint32_t *p_remaining_bytes );


	/*********************************************************************
	 * Function: addNtbaSection
	 *
	 * Purpose: add NTBA read-only section to secure store file
	 *
	 * Inputs: bufPtrPtr - pointer to pointer to buffer
	 *         maxLength - bytes available in *BufPtrPtr
	 *         idFilename - idfile name/path
	 *
	 * Outputs: Updates stores data at *BufPtrPtr, updates *BufPtrPtr
	 *
	 * returns: 0 if everything okay
	 *          -1 otherwise
	 *
	 *********************************************************************/
	static int addNtbaSection( uint8_t **p_data, uint32_t *p_remaining_bytes, const std::string &idFilename);

    static int processEsn(FILE *ifp, uint8_t **p_data, uint32_t *p_remaining_bytes);
    static int processKpe(FILE *ifp, uint8_t **p_data, uint32_t *p_remaining_bytes);
    static int processKph(FILE *ifp, uint8_t **p_data, uint32_t *p_remaining_bytes);

	 /*********************************************************************
	 * Function: AddDrmSection
	 *
	 * Purpose: add DRM read-only section to secure store file
	 *
	 * Inputs: bufPtrPtr - pointer to pointer to buffer
	 *         maxLength - bytes available in *BufPtrPtr
	 *         drmDir -    name of directory that contains drm files
	 *
	 * Outputs: Updates stores data at *BufPtrPtr, updates *BufPtrPtr
	 *
	 * returns: 0 if everything okay
	 *          -1 otherwise
	 *
	 *********************************************************************/
	static int addDrmSection( uint8_t **p_data, uint32_t *p_remaining_bytes, const std::string &drmDir);

    static int addDrmSectionItem( uint8_t **p_data, uint32_t *p_remaining_bytes, const std::string &drmDir, const char *fileName, drm_RO_AgentClientKeys keyId );

	/*********************************************************************
	 * Function: aesCbc128InPlaceEncryptZeroIV
	 *
	 * Purpose:  Do AES 128 CBC in-place encryption on passed buffer
	 *           that has already been padded (if necessary)
	 *
	 * Inputs:  cleartextPtr - pointer to cleartext
	 *          cleartextLength - length of cleartext
	 *          aesKeyPtr - pointer to 128-bit AES key
	 *
	 * Outputs: encrypts data, overwrites input buffer
	 *
	 *
	 * returns: 0 if everything okay
	 *          -1 otherwise
	 *
	 *********************************************************************/
	static int aesCbc128EncryptZeroIV( uint8_t     *cleartextPtr,
	                            uint32_t    cleartextLength,
	                            uint8_t     *aesKeyPtr );

	/*********************************************************************
	 * Function: sha1
	 *
	 * Purpose:  Do sha1 on passed buffer
	 *
	 * Inputs:  dataPtr - pointer to cleartext
	 *          dataLength - length of cleartext
	 *          resultPtr - pointer to buffer for result
	 *
	 * Outputs: Writes Sha1 value at *ResltPtr
	 *
	 *
	 * returns: 0 always (asserts otherwise)
	 *
	 *********************************************************************/
	static int sha1( uint8_t     *dataPtr,
	          uint32_t    dataLength,
	          uint8_t     *resultPtr );

	/*********************************************************************
	 * Function: hmacSha256
	 *
	 * Purpose:  Do hmacSha256 on passed buffer
	 *
	 * Inputs:  dataPtr - pointer to cleartext
	 *          dataLength - length of cleartext
	 *          resultPtr - pointer to buffer for result
	 *
	 * Outputs: Writes HMAC value at *ResltPtr
	 *
	 *
	 * returns: Length of HMAC (256/8) if everything okay
	 *          -1 otherwise
	 *
	 *********************************************************************/
	static int hmacSha256( uint8_t     *dataPtr,
	                uint32_t    dataLength,
	                uint8_t     *keyPtr,
	                uint32_t    keyLength,
	                uint8_t     *resultPtr );

    /*********************************************************************
     * Function: checkDrmFiles
     *
     * Purpose:  Verifies if the 3 required DRM files exist
     *
     * Inputs:   drmDir - name of directory that contains drm files
     *
     *********************************************************************/
    static int checkDrmFiles(const std::string &drmDir);

	static int b64_decode( char *inPtr, char *outPtr );

	static void decodeblock( unsigned char *in, unsigned char *out );

	// removes the trailing \n, \r or \r\n characters from end
	static void trimNewLine(char *str);

	// private variables
	static uint8_t secStoreHmacKey[];
	static uint8_t secStoreAesKey[];
	static uint8_t fileBuffer[];
	static char readBuffer[];
	// Translation Tables for base64 encode/decode as described in RFC1113
	static const char cb64[];
	static const char cd64[];
};

}
}

#endif
