/***********************************************************
 * YUVDEINTERLACER for the mjpegtools                      *
 * ------------------------------------------------------- *
 * (C) 2001-2004 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 ***********************************************************/

/* dedicated to my grand-pa */

/* PLANS: I would like to be able to fill any kind of yuv4mpeg-format into 
 *        the deinterlacer. Mpeg2enc (all others, too??) doesn't seem to
 *        like interlaced material. Progressive frames compress much
 *        better... Despite that pogressive frames do look so much better
 *        even when viewed on a TV-Screen, that I really don't like to 
 *        deal with that weird 1920's interlacing sh*t...
 *
 *        I would like to support 4:2:2 PAL/NTSC recordings as well as 
 *        4:1:1 PAL/NTSC recordings to come out as correct 4:2:0 progressive-
 *        frames. I expect to get better colors and a better motion-search
 *        result, when using these two as the deinterlacer's input...
 *
 *        When I really get this working reliably (what I hope as I now own
 *        a digital PAL-Camcorder, but interlaced (progressive was to expen-
 *        sive *sic*)), I hope that I can get rid of that ugly interlacing
 *        crap in the denoiser's core. 
 */

/* write SADs out to "SAD-statistics.data"
 *
 * I used this, to verify the theory I had about the statistic distribution of 
 * good and perfect SADs. The distribution is not gaussian, so we can't use the
 * mean-value, to distinguish between good and bad hits -- but well, my first
 * approach to use the interquantile-distances (25% and 75% of the distribution)
 * was wrong, too... 
 * The only working solution I was able to find, was to compare to half and double
 * of the median-SAD. I don't think, that this is proper statistics, but ... it
 * seem's to work...
 *
 * #define STATFILE
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "cpu_accel.h"
#include "motionsearch.h"
#include "sinc_interpolation.h"
#include "interleave.h"
#include "copy_frame.h"
#include "blend_fields.h"
#include "transform_block.h"

#ifdef STATFILE
FILE * statistics;
#endif

int BLKmedian    =256;

uint32_t predicted_count=0;
uint32_t search_count=0;

int search_radius=32;
int verbose = 0;
int fast_mode = 0;
int field_order = -1;
int bttv_hack = 0;
int width = 0;
int height = 0;
int input_chroma_subsampling = 0;
int output_chroma_subsampling = 0;
int non_interleaved_fields = 0;

struct vector forward_vector[768/16][576/16];
struct vector last_forward_vector[768/16][576/16];
struct vector last_forward_vector2[768/16][576/16];

uint8_t fiy	[1024 * 768];
uint8_t ficr[1024 * 768];
uint8_t ficb[1024 * 768];
uint8_t *inframe[3] = { fiy, ficr, ficb };

uint8_t foy	[1024 * 768];
uint8_t focr[1024 * 768];
uint8_t focb[1024 * 768];
uint8_t *outframe[3] = { foy, focr, focb };

uint8_t _f1y_	[1024 * 768];
uint8_t _f1cr_[1024 * 768];
uint8_t _f1cb_[1024 * 768];
uint8_t *_frame1_[3] = { _f1y_, _f1cr_, _f1cb_ };

uint8_t _f2y_	[1024 * 768];
uint8_t _f2cr_[1024 * 768];
uint8_t _f2cb_[1024 * 768];
uint8_t *_frame2_[3] = { _f2y_, _f2cr_, _f2cb_ };

uint8_t f1y	[1024 * 768];
uint8_t f1cr[1024 * 768];
uint8_t f1cb[1024 * 768];
uint8_t *frame1[3] = { f1y, f1cr, f1cb };

uint8_t f2y	[1024 * 768];
uint8_t f2cr[1024 * 768];
uint8_t f2cb[1024 * 768];
uint8_t *frame2[3] = { f2y, f2cr, f2cb };

uint8_t f3y	[1024 * 768];
uint8_t f3cr[1024 * 768];
uint8_t f3cb[1024 * 768];
uint8_t *frame3[3] = { f3y, f3cr, f3cb };

uint8_t f4y	[1024 * 768];
uint8_t f4cr[1024 * 768];
uint8_t f4cb[1024 * 768];
uint8_t *frame4[3] = { f4y, f4cr, f4cb };

uint8_t f3y_s1	[1024 * 768];
uint8_t f3cr_s1 [1024 * 768];
uint8_t f3cb_s1	[1024 * 768];
uint8_t *frame3_sub1[3] = { f3y_s1, f3cr_s1, f3cb_s1 };

uint8_t f4y_s1	[1024 * 768];
uint8_t f4cr_s1 [1024 * 768];
uint8_t f4cb_s1	[1024 * 768];
uint8_t *frame4_sub1[3] = { f4y_s1, f4cr_s1, f4cb_s1 };

uint8_t f3y_s2	[1024 * 768];
uint8_t f3cr_s2 [1024 * 768];
uint8_t f3cb_s2	[1024 * 768];
uint8_t *frame3_sub2[3] = { f3y_s2, f3cr_s2, f3cb_s2 };

uint8_t f4y_s2	[1024 * 768];
uint8_t f4cr_s2 [1024 * 768];
uint8_t f4cb_s2	[1024 * 768];
uint8_t *frame4_sub2[3] = { f4y_s2, f4cr_s2, f4cb_s2 };

uint8_t ry	[1024 * 768];
uint8_t rcr [1024 * 768];
uint8_t rcb	[1024 * 768];
uint8_t *reconstructed[3] = { ry, rcr, rcb };

struct
{
	int x;
	int y;
	uint32_t SAD;
} mv_table[1024][1024];

struct
{
	int x;
	int y;
	uint32_t SAD;
} mv_table2[1024][1024];

struct vector
{
	int x;
	int y;
	uint32_t sad;
};

struct vector search_forward_vector(int , int);

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

void motion_compensate_field 	(void);
void (*blend_fields)			(uint8_t * dst[3], uint8_t * src1[3], uint8_t * src2[3]);

/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int
main (int argc, char *argv[])
{
	int cpucap = cpu_accel();
	int framenr = 0;
	char c;
	int fd_in = 0;
	int fd_out = 1;
	int errno = 0;
	y4m_frame_info_t iframeinfo;
	y4m_stream_info_t istreaminfo;
	y4m_frame_info_t oframeinfo;
	y4m_stream_info_t ostreaminfo;

#ifdef STATFILE
	statistics = fopen("SAD-statistics.data","w");
#endif

	blend_fields = &blend_fields_non_accel;

	mjpeg_log (LOG_INFO,
		   "-------------------------------------------------");
	mjpeg_log (LOG_INFO,
		   "       Motion-Compensating-Deinterlacer          ");
	mjpeg_log (LOG_INFO,
		   "-------------------------------------------------");

	while ((c = getopt (argc, argv, "hvr:s:")) != -1)
	{
		switch (c)
		{
		case 'h':
		{
			mjpeg_log (LOG_INFO,"\n\n"
				" Usage of the deinterlacer                                  \n"
				" -------------------------                                  \n"
				"                                                            \n"
				" You should use this program when you (as I do) prefer      \n"
                " motion-compensation-artefacts over interlacing artefacts   \n"
                " in your miniDV recordings. BTW, you can make them even     \n"
                " more to apear like 35mm film recordings, when you lower    \n"
                " the saturation a little (75%%-80%%) and raise the contrast \n"
                " of the luma-channel. You may (if you don't care of the     \n"
                " bitrate) add some noise and blurr the result with          \n"
                " y4mspatialfilter (but not too much ...).                   \n"
                "                                                            \n"
                " y4mdeinterlace understands the following options:          \n"
                "                                                            \n"
				" -v verbose/debug                                           \n"
				" -r [n=0...72] sets the serach-radius (default is 8)        \n"
				" -s [n=0/1] forces field-order in case of misflagged streams\n"
                "    -s0 is bottom-field-first                               \n"
                "    -s1 is bottom-field-first                               \n");

			exit (0);
			break;
		}
		case 'r':
		{
			search_radius = atoi(optarg);
			mjpeg_log (LOG_INFO,"serach-radius set to %i",search_radius);
			break;
		}
		case 'b':
		{
			bttv_hack = 1;
			break;
		}
		case 'f':
		{
			fast_mode = 1;
			break;
		}
		case 'i':
		{
			non_interleaved_fields = 1;
			break;
		}
		case 'v':
		{
			verbose = 1;
			break;
		}
		case 's':
		{
			field_order = atoi(optarg);
			if(field_order!=0) 
			{
				mjpeg_log (LOG_INFO,"forced top-field-first!");
				field_order=1;
			}
			else
			{
				mjpeg_log (LOG_INFO,"forced bottom-field-first!");
			}
			break;
		}
		}
	}

	/* initialize motion_library */
	init_motion_search ();

	/* initialize MMX transforms (fixme) */
	if( (cpucap & ACCEL_X86_MMXEXT)!=0 ||
		(cpucap & ACCEL_X86_SSE   )!=0 )
	{
		mjpeg_log (LOG_INFO,"FIXME: could use MMX/SSE Block/Frame-Copy/Blend if I had one ;-)");
	}

	/* initialize stream-information */
	y4m_accept_extensions(1);
	y4m_init_stream_info (&istreaminfo);
	y4m_init_frame_info (&iframeinfo);
	y4m_init_stream_info (&ostreaminfo);
	y4m_init_frame_info (&oframeinfo);

	/* open input stream */
	if ((errno = y4m_read_stream_header (fd_in, &istreaminfo)) != Y4M_OK)
	{
		mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!",
			   y4m_strerr (errno));
		exit (1);
	}

	/* get format information */
	width = y4m_si_get_width (&istreaminfo);
	height = y4m_si_get_height (&istreaminfo);
	input_chroma_subsampling = y4m_si_get_chroma (&istreaminfo);
	mjpeg_log (LOG_INFO, "Y4M-Stream is %ix%i(%s)",
		width,
		height,
		input_chroma_subsampling==Y4M_CHROMA_420JPEG  ? "4:2:0 MPEG1"   :
		input_chroma_subsampling==Y4M_CHROMA_420MPEG2 ? "4:2:0 MPEG2"   :
		input_chroma_subsampling==Y4M_CHROMA_420PALDV ? "4:2:0 PAL-DV"  :
		input_chroma_subsampling==Y4M_CHROMA_444      ? "4:4:4"         :
		input_chroma_subsampling==Y4M_CHROMA_422      ? "4:2:2"         :
		input_chroma_subsampling==Y4M_CHROMA_411      ? "4:1:1 NTSC-DV" :
		input_chroma_subsampling==Y4M_CHROMA_MONO     ? "MONOCHROME"    :
		input_chroma_subsampling==Y4M_CHROMA_444ALPHA ? "4:4:4:4 ALPHA" : 
		"unknown" );

	/* if chroma-subsampling isn't supported bail out ... */
	if( input_chroma_subsampling==Y4M_CHROMA_MONO     ||
		input_chroma_subsampling==Y4M_CHROMA_444ALPHA )
	{
		exit (-1);
	}

	/* check for the field-order or if the fieldorder was forced */
	if(field_order==-1)
	{
		if (Y4M_ILACE_TOP_FIRST == y4m_si_get_interlace (&istreaminfo))
		{
			mjpeg_log (LOG_INFO, "top-field-first");
			field_order = 1;
		}
		else if (Y4M_ILACE_BOTTOM_FIRST == y4m_si_get_interlace (&istreaminfo))
		{
			mjpeg_log (LOG_INFO, "bottom-field-first");
			field_order = 0;
		}
		else
		{
			mjpeg_log (LOG_WARN, "stream either is marked progressive");
			mjpeg_log (LOG_WARN, "or stream has unknown field order");
			mjpeg_log (LOG_WARN, "using top-field-first");
			field_order = 1;
		}
	}

	/* the output is not interlaced 4:2:0 MPEG 1 */
	y4m_si_set_interlace    (&ostreaminfo, Y4M_ILACE_NONE);
	y4m_si_set_chroma       (&ostreaminfo, Y4M_CHROMA_420JPEG);
	y4m_si_set_width        (&ostreaminfo, width);
	y4m_si_set_height       (&ostreaminfo, height);
	y4m_si_set_framerate    (&ostreaminfo, y4m_si_get_framerate(&istreaminfo));
	y4m_si_set_sampleaspect (&ostreaminfo, y4m_si_get_sampleaspect(&istreaminfo));

	/* write the outstream header */
	y4m_write_stream_header (fd_out, &ostreaminfo);

	/* read every frame until the end of the input stream and process it */
	while (Y4M_OK == (errno = y4m_read_frame (fd_in,
						  &istreaminfo,
						  &iframeinfo, inframe)))
	{
		/* reconstruct 4:4:4 full resolution frame by upsampling luma and chroma */

		if( input_chroma_subsampling == Y4M_CHROMA_420JPEG )
		{
			/* chroma is handled like luma in this case ... */

			/* interpolate first frame */
			sinc_interpolation_luma (frame1[0],inframe[0],field_order);

			sinc_interpolation_luma (_frame1_[0],inframe[0],field_order);
			interpolation_420JPEG_to_444_chroma (_frame1_[1],inframe[1],field_order);
			interpolation_420JPEG_to_444_chroma (_frame1_[2],inframe[2],field_order);

			/* interpolate second frame */
			sinc_interpolation_luma (frame2[0],inframe[0],1-field_order);
			sinc_interpolation_luma (_frame2_[0],inframe[0],1-field_order);
			interpolation_420JPEG_to_444_chroma (_frame2_[1],inframe[1],1-field_order);
			interpolation_420JPEG_to_444_chroma (_frame2_[2],inframe[2],1-field_order);
		}
		else
			if( input_chroma_subsampling == Y4M_CHROMA_420MPEG2 )
			{
				/* interpolate first frame */
				sinc_interpolation_luma (frame1[0],inframe[0],field_order);
				interpolation_420MPEG2_to_444_chroma (frame1[1],inframe[1],field_order);
				interpolation_420MPEG2_to_444_chroma (frame1[2],inframe[2],field_order);
	
				/* interpolate second frame */
				sinc_interpolation_luma (frame2[0],inframe[0],1-field_order);
				interpolation_420MPEG2_to_444_chroma (frame2[1],inframe[1],1-field_order);
				interpolation_420MPEG2_to_444_chroma (frame2[2],inframe[2],1-field_order);
			}
			else
				if( input_chroma_subsampling == Y4M_CHROMA_420PALDV )
				{
					/* interpolate first frame */
					sinc_interpolation_luma (frame1[0],inframe[0],field_order);
					interpolation_420PALDV_to_444_chroma (frame1[1],inframe[1],field_order);
					interpolation_420PALDV_to_444_chroma (frame1[2],inframe[2],field_order);

					/* interpolate second frame */
					sinc_interpolation_luma (frame2[0],inframe[0],1-field_order);
					interpolation_420PALDV_to_444_chroma (frame2[1],inframe[1],1-field_order);
					interpolation_420PALDV_to_444_chroma (frame2[2],inframe[2],1-field_order);
				}
				else
					if( input_chroma_subsampling == Y4M_CHROMA_444 )
					{
						/* chroma is handled like luma in this case ... */

						/* interpolate first frame */
						sinc_interpolation_luma (frame1[0],inframe[0],field_order);
						sinc_interpolation_luma (frame1[1],inframe[1],field_order);
						sinc_interpolation_luma (frame1[2],inframe[2],field_order);

						/* interpolate second frame */
						sinc_interpolation_luma (frame2[0],inframe[0],1-field_order);
						sinc_interpolation_luma (frame2[1],inframe[1],1-field_order);
						sinc_interpolation_luma (frame2[2],inframe[2],1-field_order);
					}
					else
						if( input_chroma_subsampling == Y4M_CHROMA_422 )
						{
							/* interpolate first frame */
							sinc_interpolation_luma (frame1[0],inframe[0],field_order);
							interpolation_422_to_444_chroma (frame1[1],inframe[1],field_order);
							interpolation_422_to_444_chroma (frame1[2],inframe[2],field_order);

							/* interpolate second frame */
							sinc_interpolation_luma (frame2[0],inframe[0],1-field_order);
							interpolation_422_to_444_chroma (frame2[1],inframe[1],1-field_order);
							interpolation_422_to_444_chroma (frame2[2],inframe[2],1-field_order);
						}
						else
							if( input_chroma_subsampling == Y4M_CHROMA_411 )
							{
								/* interpolate first frame */
								sinc_interpolation_luma (frame1[0],inframe[0],field_order);
								interpolation_411_to_444_chroma (frame1[1],inframe[1],field_order);
								interpolation_411_to_444_chroma (frame1[2],inframe[2],field_order);
	
								/* interpolate second frame */
								sinc_interpolation_luma (frame2[0],inframe[0],1-field_order);
								interpolation_411_to_444_chroma (frame2[1],inframe[1],1-field_order);
								interpolation_411_to_444_chroma (frame2[2],inframe[2],1-field_order);
							}			

		/* subsample by 2 */
		subsample (frame4_sub1[0], frame2[0]);
		subsample (frame3_sub1[0], frame1[0]);

		/* subsample by 4 */
		subsample (frame4_sub2[0], frame4_sub1[0]);
		subsample (frame3_sub2[0], frame3_sub1[0]);

		/* try to motion-compensate the next-field to the current */
		motion_compensate_field();

		/* mix current and motion-compensated field to a full frame */
		blend_fields (outframe,_frame1_,reconstructed);

		/* downscale chroma to 420 JPEG/MPEG1 */
		C444_to_C420 ( outframe[1], outframe[1] );
		C444_to_C420 ( outframe[2], outframe[2] );

 		/* write out reconstructed fields */
		if(framenr++>0)
		{
	        y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, outframe);
//			memset ( frame1[1], 128, width*height);
//			memset ( frame1[2], 128, width*height);
//	        y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame1);
		}

		/* rotate field-buffers */
		memcpy ( frame3[0], frame1[0], width*height );
		memcpy ( frame3[1], frame1[1], width*height );
		memcpy ( frame3[2], frame1[2], width*height );
		memcpy ( frame4[0], frame2[0], width*height );
		memcpy ( frame4[1], frame2[1], width*height );
		memcpy ( frame4[2], frame2[2], width*height );
	}

	/* did stream end unexpectedly ? */
	if (errno != Y4M_ERR_EOF)
		mjpeg_error_exit1 ("%s", y4m_strerr (errno));

	/* Exit gently */
	return (0);
}

#if 0
inline struct vector medianvector (struct vector v1, struct vector v2, struct vector v3)
{
	struct vector v;
	v.x = 	(v1.x <= v2.x && v1.x >= v3.x ) ? v1.x :
			(v2.x <= v1.x && v2.x >= v3.x ) ? v2.x :
			v3.x ;
	v.y = 	(v1.y <= v2.y && v1.y >= v3.y ) ? v1.y :
			(v2.y <= v1.y && v2.y >= v3.y ) ? v2.y :
			v3.y ;
	v.sad =	(v1.sad <= v2.sad && v1.sad >= v3.sad ) ? v1.sad :
			(v2.sad <= v1.sad && v2.sad >= v3.sad ) ? v2.sad :
			v3.sad ;
	return v;
}

int qsort_cmp_medianSAD ( const void * x, const void * y )
{
	int a = *(const uint32_t*)x;
	int b = *(const uint32_t*)y;
	if (a<b)  return -1;
	else
		if (a>b)  return  1;
		else
			return 0;
}

uint32_t medianSAD (uint32_t * SADlist, int n)
{
	qsort ( SADlist, n, sizeof(SADlist[0]), qsort_cmp_medianSAD);

	return (SADlist[n/2]+SADlist[n/2+1])>>1;
}

inline uint32_t SADfunction ( uint8_t * f1, uint8_t * f2 )
{
	f1 -= 8+8*width;
	f2 -= 8+8*width;
	return psad_00( f1, f2, width, 32, 0 )+psad_00( f1+16, f2+16, width, 32, 0 );
}
#endif

struct vector
search_forward_vector( int x, int y )
{
	int dx,dy;
	int x2=x/2;
	int y2=y/2;
	int x4=x/4;
	int y4=y/4;
	struct vector v,h;
	static struct vector me_candidates1[50];
	int cc1=0; // candidate count
	static struct vector me_candidates2[50];
	int cc2=0; // candidate count
	int i;
	int max_candidates = 8;
	uint32_t SAD;
	uint32_t min;

	/* subsampled full-search with radius 16*4=64 */
	min = psad_sub44 ( frame3_sub2[0]+(x4-1)+(y4-1)*width, frame4_sub2[0]+(x4-1)+(y4-1)*width, width, 4 );
	me_candidates1[0].x = 0;
	me_candidates1[0].y = 0;
	cc1 = 1;
	for(dy=(-search_radius/4);dy<=(search_radius/4);dy++)
		for(dx=-search_radius/4;dx<=(search_radius/4);dx++)
		{
			SAD  = psad_sub44 ( frame3_sub2[0]+(x4-1)+(y4-1)*width, frame4_sub2[0]+(x4+dx-1)+(y4+dy-1)*width, width, 4 );

			if(SAD <= min)
			{
				/* store new search-minimum */
				min = SAD;

				/* store possible new candidate but also save the previous 24 candidates */
				me_candidates1[cc1].x = dx;
				me_candidates1[cc1].y = dy;

				/* if the maximum candidate-index (0..24) is reached, roll over candidates...
				 * instead of increasing the index ... 
                 */
				if(cc1>=max_candidates) 
				{
					for(i=0;i<max_candidates;i++)
					{
						me_candidates1[i] = me_candidates1[i+1];
					}
				}
				else
				{
					cc1++;
				}
			} 
		}

	/* subsampled reduced full-search arround sub44 candidates */
	min = psad_sub44 ( frame3_sub1[0]+(x2)+(y2)*width, frame4_sub1[0]+(x2)+(y2)*width, width, 4 );
	me_candidates2[0].x = 0;
	me_candidates2[0].y = 0;
	cc2 = 1;
	while(cc1>-1)
	{
		for(dy=(me_candidates1[cc1].y*2-1);dy<=(me_candidates1[cc1].y*2+1);dy++)
			for(dx=(me_candidates1[cc1].x*2-1);dx<=(me_candidates1[cc1].x*2+1);dx++)
			{
				SAD  = psad_sub44 ( frame3_sub1[0]+(x2)+(y2)*width, frame4_sub1[0]+(x2+dx)+(y2+dy)*width, width, 4 );

				if(SAD <= min)
				{
					/* store new search-minimum */
					min = SAD;

					/* store possible new candidate but also save the previous 24 candidates */
					me_candidates2[cc2].x = dx;
					me_candidates2[cc2].y = dy;
	
					/* if the maximum candidate-index (0..24) is reached, roll over candidates...
					 * instead of increasing the index ... 
	                 */
					if(cc2>=max_candidates) 
					{
						for(i=0;i<max_candidates;i++)
						{
							me_candidates2[i] = me_candidates2[i+1];
						}
						
					}
					else
					{
						cc2++;
					}
				} 
			}
		cc1--;
	}

	/* reduced full-search arround sub22 candidates */

	min  = psad_00 ( frame1[0]+(x-4)+(y-4)*width, frame2[0]+(x-4)+(y-4)*width, width, 16, 0x00ffffff  );
//	min  = psad_sub22 ( frame1[0]+(x)+(y)*width, frame2[0]+(x)+(y)*width, width, 8 );

	v.x = 0;
	v.y = 0;
	while(cc2>-1)
	{
		dy=me_candidates2[cc2].y*2;
//		for(dy=(me_candidates2[cc2].y*2-1);dy<=(me_candidates2[cc2].y*2+1);dy++)
			for(dx=(me_candidates2[cc2].x*2-1);dx<=(me_candidates2[cc2].x*2+1);dx++)
			{
				SAD   = psad_00 ( frame1[0]+(x-4)+(y-4)*width, frame2[0]+(x+dx-4)+(y+dy-4)*width, width, 16, 0x00ffffff );
//				SAD   = psad_sub22 ( frame1[0]+(x)+(y)*width, frame2[0]+(x+dx)+(y+dy)*width, width, 16 );
				SAD  += dy*dy + dx*dx ;
				if( SAD<min )
				{
					/* store new search-minimum */
					min   = SAD;
					v.x = dx;
					v.y = dy;
				} 
			}
		cc2--;
    }

	return v; /* return the found vector */
}

void
motion_compensate_field (void)
{
	int x,y;
	int dx,dy;
	struct vector forward_vector;

	/* search the vectors */
	for(y=0;y<height;y+=8)
	{
		for(x=0;x<width;x+=8)
		{
			forward_vector = search_forward_vector ( x, y );

			dx = forward_vector.x;
			dy = forward_vector.y;

			transform_block  (reconstructed[0]+x+y*width,
							  _frame2_[0]+(x+dx)+(y+dy)*width, width );
			transform_block  (reconstructed[1]+x+y*width,
							  _frame2_[1]+(x+dx)+(y+dy)*width, width );
			transform_block  (reconstructed[2]+x+y*width,
							  _frame2_[2]+(x+dx)+(y+dy)*width, width );
		}
	}
}

