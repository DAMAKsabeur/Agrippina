#ifndef _WTOOL
#define _WTOOL

//Header
#define IW15_MAX_FREQUENCIES	16
#define IW15_MAX_BITRATES	8
#define IW15_MAX_TXPOWER	8
#define IW15_MAX_ENCODING_SIZES	8
#define IW15_MAX_SPY		8
#define IW15_MAX_AP		8

/* Some usefull constants */
#define KILO	1e3
#define MEGA	1e6
#define GIGA	1e9
/* For doing log10/exp10 without libm */
#define LOG10_MAGIC	1.25892541179
#define nflx_wifi_range_off(f)	( ((char *) &(((struct iw15_range *) NULL)->f)) - \
			  (char *) NULL)
#define nflx_wifi_short_range_off(f)	( ((char *) &(((struct iw_range *) NULL)->f)) - \
			  (char *) NULL)


typedef struct iw_statistics	iwstats;
typedef struct iw_range		iwrange;
typedef struct iw_param		iwparam;
typedef struct iw_freq		iwfreq;
typedef struct iw_quality	iwqual;
typedef struct iw_priv_args	iwprivargs;
typedef struct sockaddr		sockaddr;


typedef struct wireless_config
{
  char		name[IFNAMSIZ + 1];	/* Wireless/protocol name */
  int		has_nwid;
  iwparam	nwid;			/* Network ID */
  int		has_freq;
  double	freq;			/* Frequency/channel */
  int		freq_flags;
  int		has_key;
  unsigned char	key[IW_ENCODING_TOKEN_MAX];	/* Encoding key used */
  int		key_size;		/* Number of bytes */
  int		key_flags;		/* Various flags */
  int		has_essid;
  int		essid_on;
  char		essid[IW_ESSID_MAX_SIZE + 1];	/* ESSID (extended network) */
  int		has_mode;
  int		mode;			/* Operation mode */
} wireless_config;

struct	iw15_range
{
	__u32		throughput;
	__u32		min_nwid;
	__u32		max_nwid;
	__u16		num_channels;
	__u8		num_frequency;
	struct iw_freq	freq[IW15_MAX_FREQUENCIES];
	__s32		sensitivity;
	struct iw_quality	max_qual;
	__u8		num_bitrates;
	__s32		bitrate[IW15_MAX_BITRATES];
	__s32		min_rts;
	__s32		max_rts;
	__s32		min_frag;
	__s32		max_frag;
	__s32		min_pmp;
	__s32		max_pmp;
	__s32		min_pmt;
	__s32		max_pmt;
	__u16		pmp_flags;
	__u16		pmt_flags;
	__u16		pm_capa;
	__u16		encoding_size[IW15_MAX_ENCODING_SIZES];
	__u8		num_encoding_sizes;
	__u8		max_encoding_tokens;
	__u16		txpower_capa;
	__u8		num_txpower;
	__s32		txpower[IW15_MAX_TXPOWER];
	__u8		we_version_compiled;
	__u8		we_version_source;
	__u16		retry_capa;
	__u16		retry_flags;
	__u16		r_time_flags;
	__s32		min_retry;
	__s32		max_retry;
	__s32		min_r_time;
	__s32		max_r_time;
	struct iw_quality	avg_qual;
};


static inline int
nflx_wifi_get_ext(int			skfd,		/* Socket to the kernel */
	   const char *		ifname,		/* Device name */
	   int			request,	/* WE ID */
	   struct iwreq *	pwrq)		/* Fixed part of the request */
{
  /* Set device name */
  strncpy(pwrq->ifr_name, ifname, IFNAMSIZ);
  /* Do the request */
  return(ioctl(skfd, request, pwrq));
}


union iw_range_raw
{
	struct iw15_range	range15;	/* WE 9->15 */
	struct iw_range		range;		/* WE 16->current */
};


double
nflx_wifi_freq2float(const iwfreq *	in)
{
  int		i;
  double	res = (double) in->m;
  for(i = 0; i < in->e; i++)
    res *= 10;
  return(res);
}


int
nflx_wifi_freq_to_channel(double			freq,
		   const struct iw_range *	range)
{
  double	ref_freq;
  int		k;

  /* Check if it's a frequency or not already a channel */
  if(freq < KILO)
    return(-1);

  for(k = 0; k < range->num_frequency; k++)
    {
      ref_freq = nflx_wifi_freq2float(&(range->freq[k]));
      if(freq == ref_freq)
	return(range->freq[k].i);
    }
  /* Not found */
  return(-2);
}


int
nflx_wifi_get_range_info(int		skfd,
		  const char *	ifname,
		  iwrange *	range)
{
  struct iwreq		wrq;
  char			buffer[sizeof(iwrange) * 2];
  union iw_range_raw *	range_raw;

  bzero(buffer, sizeof(buffer));

  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = sizeof(buffer);
  wrq.u.data.flags = 0;
  if(nflx_wifi_get_ext(skfd, ifname, SIOCGIWRANGE, &wrq) < 0)
    return(-1);

  range_raw = (union iw_range_raw *) buffer;

  if(wrq.u.data.length < 300)
    {
      range_raw->range.we_version_compiled = 9;
    }

  if(range_raw->range.we_version_compiled > 15)
    {
      memcpy((char *) range, buffer, sizeof(iwrange));
    }
  else
    {
      bzero((char *) range, sizeof(struct iw_range));
      memcpy((char *) range,
	     buffer,
	     nflx_wifi_range_off(num_channels));
      memcpy((char *) range + nflx_wifi_short_range_off(num_channels),
	     buffer + nflx_wifi_range_off(num_channels),
	     nflx_wifi_range_off(sensitivity) - nflx_wifi_range_off(num_channels));
      memcpy((char *) range + nflx_wifi_short_range_off(sensitivity),
	     buffer + nflx_wifi_range_off(sensitivity),
	     nflx_wifi_range_off(num_bitrates) - nflx_wifi_range_off(sensitivity));
      memcpy((char *) range + nflx_wifi_short_range_off(num_bitrates),
	     buffer + nflx_wifi_range_off(num_bitrates),
	     nflx_wifi_range_off(min_rts) - nflx_wifi_range_off(num_bitrates));
      memcpy((char *) range + nflx_wifi_short_range_off(min_rts),
	     buffer + nflx_wifi_range_off(min_rts),
	     nflx_wifi_range_off(txpower_capa) - nflx_wifi_range_off(min_rts));
      memcpy((char *) range + nflx_wifi_short_range_off(txpower_capa),
	     buffer + nflx_wifi_range_off(txpower_capa),
	     nflx_wifi_range_off(txpower) - nflx_wifi_range_off(txpower_capa));
      memcpy((char *) range + nflx_wifi_short_range_off(txpower),
	     buffer + nflx_wifi_range_off(txpower),
	     nflx_wifi_range_off(avg_qual) - nflx_wifi_range_off(txpower));
      memcpy((char *) range + nflx_wifi_short_range_off(avg_qual),
	     buffer + nflx_wifi_range_off(avg_qual),
	     sizeof(struct iw_quality));
    }


  return(0);
}

#endif

