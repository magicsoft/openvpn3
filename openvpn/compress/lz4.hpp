#ifndef OPENVPN_COMPRESS_LZ4_H
#define OPENVPN_COMPRESS_LZ4_H

// Implement LZ4 compression.
// Should only be included by compress.hpp

#include <algorithm> // for std::max

#include <lz4.h>

namespace openvpn {

  class CompressLZ4 : public Compress
  {
    // magic number for LZ4 compression
    enum {
      LZ4_COMPRESS = 0x69,
    };

  public:
    CompressLZ4(const Frame::Ptr& frame, const ProtoStats::Ptr& stats)
      : Compress(frame, stats)
    {
    }

  private:
    virtual void compress(BufferAllocated& buf, const bool hint)
    {
      // skip null packets
      if (!buf.size())
	return;

      if (hint)
	{
	  // initialize work buffer
	  frame->prepare(Frame::COMPRESS_WORK, work);

	  // verify that input data length is not too large
	  if (lz4_extra_buffer(buf.size()) > work.max_size())
	    {
	      error(buf);
	      return;
	    }

	  // do compress
	  const int comp_size = LZ4_compress((char *)buf.c_data(), (char *)work.data(), (int)buf.size());

	  // did compression actually reduce data length?
	  if (comp_size < buf.size())
	    {
	      if (comp_size < 0)
		{
		  error(buf);
		  return;
		}
	      work.set_size(comp_size);
	      do_swap(work, LZ4_COMPRESS);
	      buf.swap(work);
	      OPENVPN_LOG_COMPRESS("LZ4 compress");
	      return;
	    }
	}

      // indicate that we didn't compress
      do_swap(buf, NO_COMPRESS_SWAP);
    }

    virtual void decompress(BufferAllocated& buf)
    {
      // skip null packets
      if (!buf.size())
	return;

      const unsigned char c = buf.pop_front();
      switch (c)
	{
	case NO_COMPRESS_SWAP:
	  do_unswap(buf);
	  break;
	case LZ4_COMPRESS:
	  {
	    do_unswap(buf);

	    // initialize work buffer
	    const int payload_size = frame->prepare(Frame::DECOMPRESS_WORK, work);

	    // do uncompress
	    const int decomp_size = LZ4_uncompress_unknownOutputSize((const char *)buf.c_data(), (char *)work.data(), (int)buf.size(), payload_size);
	    if (decomp_size < 0)
		{
		  error(buf);
		  return;
		}
	    work.set_size(decomp_size);
	    buf.swap(work);
	    OPENVPN_LOG_COMPRESS("LZ4 uncompress");
	  }
	  break;
	default: 
	  error(buf); // unknown op
	}
    }

    // worst case size expansion on compress
    // from LZ4 docs: worst case size is : "inputsize + 0.4%", with "0.4%" being at least 8 bytes.
    size_t lz4_extra_buffer(const size_t len)
    {
      return len + std::max(len/128, size_t(8)); // for speed, use a more conservative 0.78%
    }

    BufferAllocated work;
  };

} // namespace openvpn

#endif // OPENVPN_COMPRESS_LZ4_H