#ifndef DECODERSTATSCOUNTER_H_
#define DECODERSTATSCOUNTER_H_

namespace netflix {
namespace device {


class DecoderStatsCounter
{
public:
    DecoderStatsCounter() :
      mNumDecoded(0),
      mNumDropped(0),
      mNumReducedQuality(0),
      mNumErrors(0)
    {
    }

    ~DecoderStatsCounter()
    {
    }

    void frameDecoded()
    {
        mNumDecoded++;
    }
    int getNumDecoded() const
    {
        return mNumDecoded;
    }

    void frameDropped()
    {
        mNumDropped++;
    }
    int getNumDropped() const
    {
        return mNumDropped;
    }

    void reducedQualityDecode()
    {
        mNumReducedQuality++;
    }
    int getNumReducedQuality() const
    {
        return mNumReducedQuality;
    }

    void decodingError()
    {
        mNumErrors++;
    }

    int  getErrorCount() const
    {
        return mNumErrors;
    }

    void resetCounters()
    {
        mNumDecoded = 0;
        mNumDropped = 0;
        mNumReducedQuality = 0;
        mNumErrors = 0;
    }

private:


    int mNumDecoded;
    int mNumDropped;
    int mNumReducedQuality;
    int mNumErrors;

};

} // namespace device
} // namespace netflix

#endif
