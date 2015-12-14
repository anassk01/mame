// license:BSD-3-Clause
// copyright-holders:David Haywood
/* */

#include "emu.h"
#include "k053936.h"

// Localized K053936/ROZ+
#define K053936_MAX_CHIPS 2

static rectangle K053936_cliprect[K053936_MAX_CHIPS];
static int K053936_offset[K053936_MAX_CHIPS][2] = {{0,0},{0,0}};
static int K053936_clip_enabled[K053936_MAX_CHIPS] = {0,0};
static int K053936_wraparound[K053936_MAX_CHIPS];




/***************************************************************************/
/*                                                                         */
/*                                 053936                                  */
/*                                                                         */
/***************************************************************************/

/*

053936
------
Evolution of the 051316. The data bus is 16-bit instead of 8-bit.
When used in "simple" mode it can generate the same effects of the 051316, but it
doesn't have internal tilemap RAM, so it just generates a couple of X/Y coordinates
indicating the pixel to display at each moment. Therefore, the tilemap and tile
sizes are not fixed.
The important addition over the 051316 is 512x4 words of internal RAM used to control
rotation and zoom scanline by scanline instead that on the whole screen, allowing for
effects like linescroll (Super Slams) or 3D rotation of the tilemap (Golfing Greats,
Premier Soccer).

control registers
000 X counter starting value / 256
001 Y counter starting value / 256
002 ["simple" mode only] amount to add to the X counter after each line (0 = no rotation)
003 ["simple" mode only] amount to add to the Y counter after each line
004 ["simple" mode only] amount to add to the X counter after each horizontal pixel
005 ["simple" mode only] amount to add to the Y counter after each horizontal pixel (0 = no rotation)
006 x------- -------- when set, register (line*4)+2 must be multiplied by 256
    -x------ -------- when set, registers 002 and 003 must be multiplied by 256
    --xxxxxx -------- clipping for the generated address? usually 3F, Premier Soccer
                      sets it to 07 before penalty kicks
    -------- x------- when set, register (line*4)+3 must be multiplied by 256
    -------- -x------ when set, registers 004 and 005 must be multiplied by 256
    -------- --xxxxxx clipping for the generated address? usually 3F, Premier Soccer
                      sets it to 0F before penalty kicks
007 -------- -x------ enable "super" mode
    -------- --x----- unknown (enable address clipping from register 006?)
    -------- ---x---- unknown
    -------- ------x- (not sure) enable clipping with registers 008-00b
008 min x screen coordinate to draw to (only when enabled by register 7)
009 max x screen coordinate to draw to (only when enabled by register 7)
00a min y screen coordinate to draw to (only when enabled by register 7)
00b max y screen coordinate to draw to (only when enabled by register 7)
00c unknown
00d unknown
00e unknown
00f unknown

additional control from extra RAM:
(line*4)+0 X counter starting value / 256 (add to register 000)
(line*4)+1 Y counter starting value / 256 (add to register 001)
(line*4)+2 amount to add to the X counter after each horizontal pixel
(line*4)+3 amount to add to the Y counter after each horizontal pixel

		  }}}*/
/*
  racinfrc writes:
0c = 7f7f
0e = 0072
00 = 0000
02 = 0000
04 = 0000
06 = 0008
08 = 0008
0a = 0000
10 = 003f
12 = 01c1
14 = 0010
16 = 010f
18 = 0010
1a = 0000
1c = 0000

(c08476, global update)
00 = c02840.l >> 13
02 = c02844.l >> 13
e20001.l = c02876.l ????

if(c02910.l)
    for(int i=0; i<0x80, i++)
        memcpy((c02901.l)+400*i, c03000+20*i, 20);
else if(c02930.l)
    for(int i=0; i<0x20, i++)
      for(int j=0; j<4, j++)
        memcpy((c02901.l)+400*j+20*i, c03000+20*i, 20);

if(c02960.l)
    src = c02960.l->m8l
	dst = roz_tilemap + c02960.l->m4l // ec0000
	for(int i=0; i<c02960.l->m2w+1; i++) {
	    p = dst + 400*i
		for(int j=0; j<c02960.l->m0w+1; j++) {
		   p.l++ = 0
		   p.l++ = src.l++
		}
	}

src1 = c02824.l   // c01000
dst1 = lram2 + 78 // e80078
src2 = c0282c.l   // c02000
dst2 = lram4 + 4  // f80004
memcpy(dst1, src1, 0xff*8)
memcpy(dst2, src2, 0xff*4)

203c36:
  c02840 = 10000000
  c02844 = 03100000

pc c0d66a includes update routine
-> c0c240

*/

DEVICE_ADDRESS_MAP_START(map, 16, k053936_device)
	AM_RANGE(0x00, 0x03) AM_WRITE(counter_start_w)
	AM_RANGE(0x04, 0x07) AM_WRITE(vadd_w)
	AM_RANGE(0x08, 0x0b) AM_WRITE(hadd_w)
	AM_RANGE(0x0c, 0x0d) AM_WRITE(mode1_w)
	AM_RANGE(0x0e, 0x0f) AM_WRITE(mode2_w)
	AM_RANGE(0x10, 0x11) AM_WRITE(x_min_w)
	AM_RANGE(0x12, 0x13) AM_WRITE(x_max_w)
	AM_RANGE(0x14, 0x15) AM_WRITE(y_min_w)
	AM_RANGE(0x16, 0x17) AM_WRITE(y_max_w)
ADDRESS_MAP_END

void k053936_device::set_rozram_tag(const char *rozram_tag)
{
	m_rozram_tag = rozram_tag;
}

WRITE16_MEMBER(k053936_device::counter_start_w)
{
	COMBINE_DATA(m_counter_start+offset);
	if(0)
		logerror("%c counter start %04x\n", 'x'+offset, m_counter_start[offset]);
}

WRITE16_MEMBER(k053936_device::vadd_w)
{
	COMBINE_DATA(m_vadd+offset);
	logerror("%c vadd %04x\n", 'x'+offset, m_vadd[offset]);
}

WRITE16_MEMBER(k053936_device::hadd_w)
{
	COMBINE_DATA(m_hadd+offset);
	logerror("%c hadd %04x\n", 'x'+offset, m_hadd[offset]);
}

WRITE16_MEMBER(k053936_device::mode1_w)
{
	COMBINE_DATA(&m_mode1);
	logerror("mode1 lx*%s hstep*%s 1=%02x ly*%s vstep*%s 2=%02x\n",
			 m_mode1 & 0x8000 ? "256" : "1",
			 m_mode1 & 0x4000 ? "256" : "1",
			 (m_mode1 & 0x3f00) >> 8,
			 m_mode1 & 0x0080 ? "256" : "1",
			 m_mode1 & 0x0040 ? "256" : "1",
			 m_mode1 & 0x003f);
}

WRITE16_MEMBER(k053936_device::mode2_w)
{
	COMBINE_DATA(&m_mode2);
	logerror("mode2 h=%02x ?=%c ram=%s ?=%c%c%c%c%c%c\n",
			 m_mode2 >> 8,
			 m_mode2 & 0x80 ? '1' : '0',
			 m_mode2 & 0x40 ? "on" : "off",
			 m_mode2 & 0x20 ? '1' : '0',
			 m_mode2 & 0x10 ? '1' : '0',
			 m_mode2 & 0x08 ? '1' : '0',
			 m_mode2 & 0x04 ? '1' : '0',
			 m_mode2 & 0x02 ? '1' : '0',
			 m_mode2 & 0x01 ? '1' : '0');
}

WRITE16_MEMBER(k053936_device::x_min_w)
{
	COMBINE_DATA(m_min + 0);
	logerror("x min %04x\n", m_min[0]);
}

WRITE16_MEMBER(k053936_device::x_max_w)
{
	COMBINE_DATA(m_max + 0);
	logerror("x max %04x\n", m_max[0]);
}

WRITE16_MEMBER(k053936_device::y_min_w)
{
	COMBINE_DATA(m_min + 1);
	logerror("y min %04x\n", m_min[1]);
}

WRITE16_MEMBER(k053936_device::y_max_w)
{
	COMBINE_DATA(m_max + 1);
	logerror("y max %04x\n", m_max[1]);
}

void k053936_device::bitmap_update(bitmap_ind16 *bitmap_x, bitmap_ind16 *bitmap_y, const rectangle &cliprect)
{
	if(m_mode2 & 0x0040) {
		uint32_t x_base = m_counter_start[0] << 8;
		uint32_t y_base = m_counter_start[1] << 8;
		for(int y = cliprect.min_y; y <= cliprect.max_y; y++) {
			uint16_t *xd = &bitmap_x->pix16(y, cliprect.min_x);
			uint16_t *yd = &bitmap_y->pix16(y, cliprect.min_x);

			uint32_t x_cur, y_cur;
			uint32_t x_delta, y_delta;

			switch(m_rozram->bytewidth()) {
			default: abort();
			case 4: {
				const uint32_t *base = (const uint32_t *)m_rozram->ptr();
				base += y << 1;
				x_cur = x_base + (base[0] >> 16);
				y_cur = y_base + (base[0] & 0xffff);
				x_delta = base[1] >> 16;
				y_delta = base[1] & 0xffff;
				break;
			}
			}
			
			x_cur += cliprect.min_y * x_delta;
			y_cur += cliprect.min_y * y_delta;
			for(int x = cliprect.min_x; x <= cliprect.max_x; x++) {
				*xd++ = (x_cur >> 8) & 0x1fff;
				*yd++ = (y_cur >> 8) & 0x1fff;
				x_cur += x_delta;
				y_cur += y_delta;
			}
		}
	} else {
		bitmap_x->fill(0, cliprect);
		bitmap_y->fill(0, cliprect);
	}
}


static void K053936_zoom_draw(int chip,uint16_t *ctrl,uint16_t *linectrl, screen_device &screen, bitmap_ind16 &bitmap,const rectangle &cliprect,tilemap_t *tmap,int flags,uint32_t priority, int glfgreat_hack)
{
	if (!tmap)
		return;

	if (ctrl[0x07] & 0x0040)
	{
		uint32_t startx,starty;
		int incxx,incxy;
		rectangle my_clip;
		int y,maxy;

		// Racin' Force will get to here if glfgreat_hack is enabled, and it ends
		// up setting a maximum y value of '13', thus causing nothing to be drawn.
		// It looks like the roz output should be flipped somehow as it seems to be
		// displaying the wrong areas of the tilemap and is rendered upside down,
		// although due to the additional post-processing the voxel renderer performs
		// it's difficult to know what the output SHOULD be.  (hold W in Racin' Force
		// to see the chip output)

		if (((ctrl[0x07] & 0x0002) && ctrl[0x09]) && (glfgreat_hack))   /* wrong, but fixes glfgreat */
		{
			my_clip.min_x = ctrl[0x08] + K053936_offset[chip][0]+2;
			my_clip.max_x = ctrl[0x09] + K053936_offset[chip][0]+2 - 1;
			if (my_clip.min_x < cliprect.min_x)
				my_clip.min_x = cliprect.min_x;
			if (my_clip.max_x > cliprect.max_x)
				my_clip.max_x = cliprect.max_x;

			y = ctrl[0x0a] + K053936_offset[chip][1]-2;
			if (y < cliprect.min_y)
				y = cliprect.min_y;
			maxy = ctrl[0x0b] + K053936_offset[chip][1]-2 - 1;
			if (maxy > cliprect.max_y)
				maxy = cliprect.max_y;

		}
		else
		{
			my_clip.min_x = cliprect.min_x;
			my_clip.max_x = cliprect.max_x;

			y = cliprect.min_y;
			maxy = cliprect.max_y;
		}

		while (y <= maxy)
		{
			uint16_t *lineaddr = linectrl + 4*((y - K053936_offset[chip][1]) & 0x1ff);
			my_clip.min_y = my_clip.max_y = y;

			startx = 256 * (int16_t)(lineaddr[0] + ctrl[0x00]);
			starty = 256 * (int16_t)(lineaddr[1] + ctrl[0x01]);
			incxx  =       (int16_t)(lineaddr[2]);
			incxy  =       (int16_t)(lineaddr[3]);

			if (ctrl[0x06] & 0x8000) incxx *= 256;
			if (ctrl[0x06] & 0x0080) incxy *= 256;

			startx -= K053936_offset[chip][0] * incxx;
			starty -= K053936_offset[chip][0] * incxy;

			tmap->draw_roz(screen, bitmap, my_clip, startx << 5,starty << 5,
					incxx << 5,incxy << 5,0,0,
					K053936_wraparound[chip],
					flags,priority);

			y++;
		}
	}
	else    /* "simple" mode */
	{
		uint32_t startx,starty;
		int incxx,incxy,incyx,incyy;

		startx = 256 * (int16_t)(ctrl[0x00]);
		starty = 256 * (int16_t)(ctrl[0x01]);
		incyx  =       (int16_t)(ctrl[0x02]);
		incyy  =       (int16_t)(ctrl[0x03]);
		incxx  =       (int16_t)(ctrl[0x04]);
		incxy  =       (int16_t)(ctrl[0x05]);

		if (ctrl[0x06] & 0x4000) { incyx *= 256; incyy *= 256; }
		if (ctrl[0x06] & 0x0040) { incxx *= 256; incxy *= 256; }

		startx -= K053936_offset[chip][1] * incyx;
		starty -= K053936_offset[chip][1] * incyy;

		startx -= K053936_offset[chip][0] * incxx;
		starty -= K053936_offset[chip][0] * incxy;

		tmap->draw_roz(screen, bitmap, cliprect, startx << 5,starty << 5,
				incxx << 5,incxy << 5,incyx << 5,incyy << 5,
				K053936_wraparound[chip],
				flags,priority);
	}

#if 0
if (machine.input().code_pressed(KEYCODE_D))
	popmessage("%04x %04x %04x %04x\n%04x %04x %04x %04x\n%04x %04x %04x %04x\n%04x %04x %04x %04x",
			ctrl[0x00],
			ctrl[0x01],
			ctrl[0x02],
			ctrl[0x03],
			ctrl[0x04],
			ctrl[0x05],
			ctrl[0x06],
			ctrl[0x07],
			ctrl[0x08],
			ctrl[0x09],
			ctrl[0x0a],
			ctrl[0x0b],
			ctrl[0x0c],
			ctrl[0x0d],
			ctrl[0x0e],
			ctrl[0x0f]);
#endif
}


void K053936_0_zoom_draw(screen_device &screen, bitmap_ind16 &bitmap,const rectangle &cliprect,tilemap_t *tmap,int flags,uint32_t priority, int glfgreat_hack)
{
	uint16_t *ctrl = reinterpret_cast<uint16_t *>(tmap->machine().root_device().memshare("k053936_0_ctrl")->ptr());
	uint16_t *linectrl = reinterpret_cast<uint16_t *>(tmap->machine().root_device().memshare("k053936_0_line")->ptr());
	K053936_zoom_draw(0,ctrl,linectrl,screen,bitmap,cliprect,tmap,flags,priority, glfgreat_hack);
}

void K053936_wraparound_enable(int chip, int status)
{
	K053936_wraparound[chip] = status;
}


void K053936_set_offset(int chip, int xoffs, int yoffs)
{
	K053936_offset[chip][0] = xoffs;
	K053936_offset[chip][1] = yoffs;
}




/***************************************************************************/
/*                                                                         */
/*                                 053936                                  */
/*                                                                         */
/***************************************************************************/

DEFINE_DEVICE_TYPE(K053936, k053936_device, "k053936", "K053936 ROZ coordinates generator")

k053936_device::k053936_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, K053936, tag, owner, clock),
	m_ctrl(nullptr),
	m_linectrl(nullptr),
	m_wrap(0),
	m_xoff(0),
	m_yoff(0)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void k053936_device::device_start()
{
	// Can't use a required_* because the pointer type varies
	m_rozram = owner()->memshare(m_rozram_tag);
	if(!m_rozram)
		fatalerror("ROZram shared memory '%s' does not exist\n", m_rozram_tag);
	if(m_rozram->bytewidth() > 4)
		fatalerror("ROZram shared memory '%s' byte width is %d, which is too large\n", m_rozram_tag, m_rozram->bytewidth());

	m_ctrl = make_unique_clear<uint16_t[]>(0x20);
	m_linectrl = make_unique_clear<uint16_t[]>(0x4000);

	save_pointer(NAME(m_ctrl.get()), 0x20);
	save_pointer(NAME(m_linectrl.get()), 0x4000);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void k053936_device::device_reset()
{
	memset(m_counter_start, 0, sizeof(m_counter_start));
	memset(m_vadd, 0, sizeof(m_vadd));
	memset(m_hadd, 0, sizeof(m_hadd));
	m_mode1 = 0x0000;
	m_mode2 = 0x0000;
	memset(m_min, 0, sizeof(m_min));
	memset(m_max, 0, sizeof(m_max));

	memset(m_ctrl.get(), 0, 0x20);
}


/*****************************************************************************
    DEVICE HANDLERS
*****************************************************************************/

WRITE16_MEMBER( k053936_device::ctrl_w )
{
	COMBINE_DATA(&m_ctrl[offset]);
}

READ16_MEMBER( k053936_device::ctrl_r )
{
	return m_ctrl[offset];
}

WRITE16_MEMBER( k053936_device::linectrl_w )
{
	COMBINE_DATA(&m_linectrl[offset]);
}

READ16_MEMBER( k053936_device::linectrl_r )
{
	return m_linectrl[offset];
}

// there is another implementation of this in  video/konamigx.c (!)
//  why? shall they be merged?
void k053936_device::zoom_draw( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, tilemap_t *tmap, int flags, uint32_t priority, int glfgreat_hack )
{
	if (!tmap)
		return;

	if (m_ctrl[0x07] & 0x0040)
	{
		uint32_t startx, starty;
		int incxx, incxy;
		rectangle my_clip;
		int y, maxy;

		// Racin' Force will get to here if glfgreat_hack is enabled, and it ends
		// up setting a maximum y value of '13', thus causing nothing to be drawn.
		// It looks like the roz output should be flipped somehow as it seems to be
		// displaying the wrong areas of the tilemap and is rendered upside down,
		// although due to the additional post-processing the voxel renderer performs
		// it's difficult to know what the output SHOULD be.  (hold W in Racin' Force
		// to see the chip output)

		if (((m_ctrl[0x07] & 0x0002) && m_ctrl[0x09]) && (glfgreat_hack)) /* wrong, but fixes glfgreat */
		{
			my_clip.min_x = m_ctrl[0x08] + m_xoff + 2;
			my_clip.max_x = m_ctrl[0x09] + m_xoff + 2 - 1;
			if (my_clip.min_x < cliprect.min_x)
				my_clip.min_x = cliprect.min_x;
			if (my_clip.max_x > cliprect.max_x)
				my_clip.max_x = cliprect.max_x;

			y = m_ctrl[0x0a] + m_yoff - 2;
			if (y < cliprect.min_y)
				y = cliprect.min_y;
			maxy = m_ctrl[0x0b] + m_yoff - 2 - 1;
			if (maxy > cliprect.max_y)
				maxy = cliprect.max_y;
		}
		else
		{
			my_clip.min_x = cliprect.min_x;
			my_clip.max_x = cliprect.max_x;

			y = cliprect.min_y;
			maxy = cliprect.max_y;
		}

		while (y <= maxy)
		{
			uint16_t *lineaddr = m_linectrl.get() + 4 * ((y - m_yoff) & 0x1ff);

			my_clip.min_y = my_clip.max_y = y;

			startx = 256 * (int16_t)(lineaddr[0] + m_ctrl[0x00]);
			starty = 256 * (int16_t)(lineaddr[1] + m_ctrl[0x01]);
			incxx  =       (int16_t)(lineaddr[2]);
			incxy  =       (int16_t)(lineaddr[3]);

			if (m_ctrl[0x06] & 0x8000)
				incxx *= 256;

			if (m_ctrl[0x06] & 0x0080)
				incxy *= 256;

			startx -= m_xoff * incxx;
			starty -= m_xoff * incxy;

			tmap->draw_roz(screen, bitmap, my_clip, startx << 5,starty << 5,
					incxx << 5,incxy << 5,0,0,
					m_wrap,
					flags,priority);

			y++;
		}
	}
	else    /* "simple" mode */
	{
		uint32_t startx, starty;
		int incxx, incxy, incyx, incyy;

		startx = 256 * (int16_t)(m_ctrl[0x00]);
		starty = 256 * (int16_t)(m_ctrl[0x01]);
		incyx  =       (int16_t)(m_ctrl[0x02]);
		incyy  =       (int16_t)(m_ctrl[0x03]);
		incxx  =       (int16_t)(m_ctrl[0x04]);
		incxy  =       (int16_t)(m_ctrl[0x05]);

		if (m_ctrl[0x06] & 0x4000)
		{
			incyx *= 256;
			incyy *= 256;
		}

		if (m_ctrl[0x06] & 0x0040)
		{
			incxx *= 256;
			incxy *= 256;
		}

		startx -= m_yoff * incyx;
		starty -= m_yoff * incyy;

		startx -= m_xoff * incxx;
		starty -= m_xoff * incxy;

		tmap->draw_roz(screen, bitmap, cliprect, startx << 5,starty << 5,
				incxx << 5,incxy << 5,incyx << 5,incyy << 5,
				m_wrap,
				flags,priority);
	}

#if 0
if (machine.input().code_pressed(KEYCODE_D))
	popmessage("%04x %04x %04x %04x\n%04x %04x %04x %04x\n%04x %04x %04x %04x\n%04x %04x %04x %04x",
			m_ctrl[0x00],
			m_ctrl[0x01],
			m_ctrl[0x02],
			m_ctrl[0x03],
			m_ctrl[0x04],
			m_ctrl[0x05],
			m_ctrl[0x06],
			m_ctrl[0x07],
			m_ctrl[0x08],
			m_ctrl[0x09],
			m_ctrl[0x0a],
			m_ctrl[0x0b],
			m_ctrl[0x0c],
			m_ctrl[0x0d],
			m_ctrl[0x0e],
			m_ctrl[0x0f]);
#endif
}




void K053936GP_set_offset(int chip, int xoffs, int yoffs) { K053936_offset[chip][0] = xoffs; K053936_offset[chip][1] = yoffs; }

void K053936GP_clip_enable(int chip, int status) { K053936_clip_enabled[chip] = status; }

void K053936GP_set_cliprect(int chip, int minx, int maxx, int miny, int maxy)
{
	rectangle &cliprect = K053936_cliprect[chip];
	cliprect.set(minx, maxx, miny, maxy);
}

static inline void K053936GP_copyroz32clip( running_machine &machine,
		bitmap_rgb32 &dst_bitmap, bitmap_ind16 &src_bitmap,
		const rectangle &dst_cliprect, const rectangle &src_cliprect,
		uint32_t _startx,uint32_t _starty,int _incxx,int _incxy,int _incyx,int _incyy,
		int tilebpp, int blend, int alpha, int clip, int pixeldouble_output, palette_device &palette )
{
	static const int colormask[8]={1,3,7,0xf,0x1f,0x3f,0x7f,0xff};
	int cy, cx;
	int ecx;
	int src_pitch, incxy, incxx;
	int src_minx, src_maxx, src_miny, src_maxy, cmask;
	uint16_t *src_base;
	size_t src_size;

	const pen_t *pal_base;
	int dst_ptr;
	int dst_size;
	int dst_base2;

	int tx, dst_pitch;
	uint32_t *dst_base;
	int starty, incyy, startx, incyx, ty, sx, sy;

	incxy = _incxy; incxx = _incxx; incyy = _incyy; incyx = _incyx;
	starty = _starty; startx = _startx;

	if (clip) // set source clip range to some extreme values when disabled
	{
		src_minx = src_cliprect.min_x;
		src_maxx = src_cliprect.max_x;
		src_miny = src_cliprect.min_y;
		src_maxy = src_cliprect.max_y;
	}
	// this simply isn't safe to do!
	else { src_minx = src_miny = -0x10000; src_maxx = src_maxy = 0x10000; }

	// set target clip range
	sx = dst_cliprect.min_x;
	tx = dst_cliprect.max_x - sx + 1;
	sy = dst_cliprect.min_y;
	ty = dst_cliprect.max_y - sy + 1;

	startx += sx * incxx + sy * incyx;
	starty += sx * incxy + sy * incyy;

	// adjust entry points and other loop constants
	dst_pitch = dst_bitmap.rowpixels();
	dst_base = &dst_bitmap.pix32(0);
	dst_base2 = sy * dst_pitch + sx + tx;
	ecx = tx = -tx;

	tilebpp = (tilebpp-1) & 7;
	pal_base = palette.pens();
	cmask = colormask[tilebpp];

	src_pitch = src_bitmap.rowpixels();
	src_base = &src_bitmap.pix16(0);
	src_size = src_bitmap.width() * src_bitmap.height();
	dst_size = dst_bitmap.width() * dst_bitmap.height();
	dst_ptr = 0;//dst_base;
	cy = starty;
	cx = startx;

	if (blend > 0)
	{
		dst_ptr += dst_pitch;      // draw blended
		starty += incyy;
		startx += incyx;

		do {
			do {
				int srcx = (cx >> 16) & 0x1fff;
				int srcy = (cy >> 16) & 0x1fff;
				int pixel;
				uint32_t offs;
				offs = srcy * src_pitch + srcx;

				cx += incxx;
				cy += incxy;

				if (offs>=src_size)
					continue;

				if (srcx < src_minx || srcx > src_maxx || srcy < src_miny || srcy > src_maxy)
					continue;

				pixel = src_base[offs];
				if (!(pixel & cmask))
					continue;

				if ((dst_ptr+ecx+dst_base2)<dst_size) dst_base[dst_ptr+ecx+dst_base2] = alpha_blend_r32(pal_base[pixel], dst_base[dst_ptr+ecx+dst_base2], alpha);

				if (pixeldouble_output)
				{
					ecx++;
					if ((dst_ptr+ecx+dst_base2)<dst_size) dst_base[dst_ptr+ecx+dst_base2] = alpha_blend_r32(pal_base[pixel], dst_base[dst_ptr+ecx+dst_base2], alpha);
				}
			}
			while (++ecx < 0);

			ecx = tx;
			dst_ptr += dst_pitch;
			cy = starty; starty += incyy;
			cx = startx; startx += incyx;
		} while (--ty);
	}
	else    //  draw solid
	{
		dst_ptr += dst_pitch;
		starty += incyy;
		startx += incyx;

		do {
			do {
				int srcx = (cx >> 16) & 0x1fff;
				int srcy = (cy >> 16) & 0x1fff;
				int pixel;
				uint32_t offs;

				offs = srcy * src_pitch + srcx;

				cx += incxx;
				cy += incxy;

				if (offs>=src_size)
					continue;

				if (srcx < src_minx || srcx > src_maxx || srcy < src_miny || srcy > src_maxy)
					continue;

				pixel = src_base[offs];
				if (!(pixel & cmask))
					continue;



				if ((dst_ptr+ecx+dst_base2)<dst_size) dst_base[dst_ptr+ecx+dst_base2] = pal_base[pixel];

				if (pixeldouble_output)
				{
					ecx++;
					if ((dst_ptr+ecx+dst_base2)<dst_size) dst_base[dst_ptr+ecx+dst_base2] = pal_base[pixel];
				}
			}
			while (++ecx < 0);

			ecx = tx;
			dst_ptr += dst_pitch;
			cy = starty; starty += incyy;
			cx = startx; startx += incyx;
		} while (--ty);
	}
}

// adapted from generic K053936_zoom_draw()
static void K053936GP_zoom_draw(running_machine &machine,
		int chip, uint16_t *ctrl, uint16_t *linectrl,
		bitmap_rgb32 &bitmap, const rectangle &cliprect, tilemap_t *tmap,
		int tilebpp, int blend, int alpha, int pixeldouble_output, palette_device &palette)
{
	uint16_t *lineaddr;

	rectangle my_clip;
	uint32_t startx, starty;
	int incxx, incxy, incyx, incyy, y, maxy, clip;

	bitmap_ind16 &src_bitmap = tmap->pixmap();
	rectangle &src_cliprect = K053936_cliprect[chip];
	clip = K053936_clip_enabled[chip];

	if (ctrl[0x07] & 0x0040)    /* "super" mode */
	{
		my_clip.min_x = cliprect.min_x;
		my_clip.max_x = cliprect.max_x;
		y = cliprect.min_y;
		maxy = cliprect.max_y;

		while (y <= maxy)
		{
			lineaddr = linectrl + ( ((y - K053936_offset[chip][1]) & 0x1ff) << 2);
			my_clip.min_y = my_clip.max_y = y;

			startx = (int16_t)(lineaddr[0] + ctrl[0x00]) << 8;
			starty = (int16_t)(lineaddr[1] + ctrl[0x01]) << 8;
			incxx  = (int16_t)(lineaddr[2]);
			incxy  = (int16_t)(lineaddr[3]);

			if (ctrl[0x06] & 0x8000) incxx <<= 8;
			if (ctrl[0x06] & 0x0080) incxy <<= 8;

			startx -= K053936_offset[chip][0] * incxx;
			starty -= K053936_offset[chip][0] * incxy;

			K053936GP_copyroz32clip(machine,
					bitmap, src_bitmap, my_clip, src_cliprect,
					startx<<5, starty<<5, incxx<<5, incxy<<5, 0, 0,
					tilebpp, blend, alpha, clip, pixeldouble_output, palette);
			y++;
		}
	}
	else    /* "simple" mode */
	{
		startx = (int16_t)(ctrl[0x00]) << 8;
		starty = (int16_t)(ctrl[0x01]) << 8;
		incyx  = (int16_t)(ctrl[0x02]);
		incyy  = (int16_t)(ctrl[0x03]);
		incxx  = (int16_t)(ctrl[0x04]);
		incxy  = (int16_t)(ctrl[0x05]);

		if (ctrl[0x06] & 0x4000) { incyx <<= 8; incyy <<= 8; }
		if (ctrl[0x06] & 0x0040) { incxx <<= 8; incxy <<= 8; }

		startx -= K053936_offset[chip][1] * incyx;
		starty -= K053936_offset[chip][1] * incyy;

		startx -= K053936_offset[chip][0] * incxx;
		starty -= K053936_offset[chip][0] * incxy;

		K053936GP_copyroz32clip(machine,
				bitmap, src_bitmap, cliprect, src_cliprect,
				startx<<5, starty<<5, incxx<<5, incxy<<5, incyx<<5, incyy<<5,
				tilebpp, blend, alpha, clip, pixeldouble_output, palette);
	}
}

void K053936GP_0_zoom_draw(running_machine &machine, bitmap_rgb32 &bitmap, const rectangle &cliprect,
		tilemap_t *tmap, int tilebpp, int blend, int alpha, int pixeldouble_output, uint16_t* temp_m_k053936_0_ctrl_16, uint16_t* temp_m_k053936_0_linectrl_16,uint16_t* temp_m_k053936_0_ctrl, uint16_t* temp_m_k053936_0_linectrl, palette_device &palette)
{
	if (temp_m_k053936_0_ctrl_16)
	{
		K053936GP_zoom_draw(machine, 0,temp_m_k053936_0_ctrl_16,temp_m_k053936_0_linectrl_16,bitmap,cliprect,tmap,tilebpp,blend,alpha, pixeldouble_output, palette);
	}
	else
	{
		K053936GP_zoom_draw(machine, 0,temp_m_k053936_0_ctrl,temp_m_k053936_0_linectrl,bitmap,cliprect,tmap,tilebpp,blend,alpha, pixeldouble_output, palette);
	}
}
