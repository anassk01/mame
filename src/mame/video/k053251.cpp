// license:BSD-3-Clause
// copyright-holders: Olivier Galibert
/*

Konami 053251 "PCU"
-------------------

Priority encoder, aka mixer.

The chip has inputs for 3 "complex" layers (CI0-2), with 9 color bits
and 6 priority bits, plus 2 "simple" layers (CI3-4) with 8 color bits
and no priority bits.  In addition it has two shadow bits (SD0-1),
where SD0 can be alternatively used as a color bit (probably on CI0
only, user by Over Drive).  It outputs a 11-bits color, two shadow
bits, one brightness bit and one "pixel present" bit.  It is fast
enough to output two pixels per pixel clock, which is used by Xexex
and Moo Mesa hardware to generate two planes for the 054338 blender.

A layer pixel is considered transparent if the 4 bottom color bits are
zero.  Lowest priority score wins.

Connections vary heavily from game to game, and Konami went very funky
on the priority bit connections, often wiring some of the bits to
vcc/gnd and connecting others to the sprite chips and friends.

14 internal registers, write-only; registers are 6 bits wide (input is
D0-D5), mostly not understood.  Register d is only used by Over Drive
(with value 0xe), probably activates the extra attribute bit.

Register map:

            5   4   3   2   1   0
00 pri 0    --------pri 0--------
01 pri 1    --------pri 1--------
02 pri 2    --------pri 2--------
03 pri 3    --------pri 3--------
04 pri 4    --------pri 4--------
05 sha0pri  -------sha0pri-------
06 sha1pri  -------sha1pri-------
07 ?        ?   ?   ?   ?   ?   ?
08 ?        ?   ?   ?   ?   ?   ?
09 cblk012  --2--   --1--   --0--
0a cblk34   ----4----   ----3----
0b ?        ?   ?   ?   ?   ?   ?
0c inpri    .   .   . in2 in1 in0
0d extsha   ?   ?   ? in2 in1 in0

pri n:   internal priority of layer <n>
cblk:    palette bits of layer <n>, 2 bits for layers 0-2, 3 for layers 4-5
sha?pri: (unproven) minimal priority value of the top layer for the shadow bits to be transmitted
inpri:   external priority active (0=yes)
extsha:  sha0 used as attribute bit (0=yes)
*/

#include "emu.h"
#include "k053251.h"
#include "kvideodac.h"

DEFINE_DEVICE_TYPE(K053251, k053251_device, "k053251", "K053251 Priority Encoder")

DEVICE_ADDRESS_MAP_START(map, 8, k053251_device)
	AM_RANGE(0x00, 0x04) AM_WRITE(pri_w)
	AM_RANGE(0x05, 0x06) AM_WRITE(sha_w)
	AM_RANGE(0x09, 0x0a) AM_WRITE(cblk_w)
	AM_RANGE(0x0c, 0x0c) AM_WRITE(inpri_w)
	AM_RANGE(0x0d, 0x0d) AM_WRITE(extsha_w)
ADDRESS_MAP_END

k053251_device::k053251_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, K053251, tag, owner, clock)
{
}

void k053251_device::set_shadow_layer(int layer)
{
	m_shadow_layer = layer;
}

WRITE8_MEMBER(k053251_device::pri_w)
{
	m_pri[offset] = data & 0x3f;
}

WRITE8_MEMBER(k053251_device::sha_w)
{
	m_sha[offset] = data & 0x3f;
}

WRITE8_MEMBER(k053251_device::cblk_w)
{
	if(!offset) {
		m_cblk[0] = (data & 0x03) << 9;
		m_cblk[1] = (data & 0x0c) << 7;
		m_cblk[2] = (data & 0x30) << 5;
	} else {
		m_cblk[3] = (data & 0x07) << 8;
		m_cblk[4] = (data & 0x38) << 5;
	}
}

WRITE8_MEMBER(k053251_device::inpri_w)
{
	m_inpri = data & 0x3f;
}

WRITE8_MEMBER(k053251_device::extsha_w)
{
	m_extsha = data & 0x3f;
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void k053251_device::device_start()
{
	m_init_cb.bind_relative_to(*owner());
	m_update_cb.bind_relative_to(*owner());
	memset(m_bitmaps, 0, sizeof(m_bitmaps));

	save_item(NAME(m_pri));
	save_item(NAME(m_inpri));
	save_item(NAME(m_cblk));
	save_item(NAME(m_sha));
	save_item(NAME(m_extsha));
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void k053251_device::device_reset()
{
	memset(m_pri, 0, sizeof(m_pri));
	memset(m_cblk, 0, sizeof(m_cblk));
	m_extsha = 0x3f;
	m_sha[0] = m_sha[1] = 0x00;
	m_inpri = 0x00;
}

void k053251_device::bitmap_update(bitmap_ind16 **bitmaps, const rectangle &cliprect)
{
	if(!m_bitmaps[0] || m_bitmaps[0]->width() != bitmaps[0]->width() || m_bitmaps[0]->height() != bitmaps[0]->height()) {
		if(m_bitmaps[0])
			for(int i=0; i<BITMAP_COUNT; i++)
				delete m_bitmaps[i];
		for(int i=0; i<BITMAP_COUNT; i++)
			m_bitmaps[i] = new bitmap_ind16(bitmaps[0]->width(), bitmaps[0]->height());
		if(!m_init_cb.isnull())
			m_init_cb(m_bitmaps);
	}

	m_update_cb(m_bitmaps, cliprect);

	u8 disp = 0x1f;
	if(machine().input().code_pressed(KEYCODE_Q)) disp &= ~0x01;
	if(machine().input().code_pressed(KEYCODE_W)) disp &= ~0x02;
	if(machine().input().code_pressed(KEYCODE_E)) disp &= ~0x04;
	if(machine().input().code_pressed(KEYCODE_R)) disp &= ~0x08;
	if(machine().input().code_pressed(KEYCODE_T)) disp &= ~0x10;

	for(int y = cliprect.min_y; y <= cliprect.max_y; y++) {
		const uint16_t *c0 = &m_bitmaps[LAYER0_COLOR]->pix16(y, cliprect.min_x);
		const uint16_t *c1 = &m_bitmaps[LAYER1_COLOR]->pix16(y, cliprect.min_x);
		const uint16_t *c2 = &m_bitmaps[LAYER2_COLOR]->pix16(y, cliprect.min_x);
		const uint16_t *c3 = &m_bitmaps[LAYER3_COLOR]->pix16(y, cliprect.min_x);
		const uint16_t *c4 = &m_bitmaps[LAYER4_COLOR]->pix16(y, cliprect.min_x);
		const uint16_t *a0 = &m_bitmaps[LAYER0_ATTR ]->pix16(y, cliprect.min_x);
		const uint16_t *a1 = &m_bitmaps[LAYER1_ATTR ]->pix16(y, cliprect.min_x);
		const uint16_t *a2 = &m_bitmaps[LAYER2_ATTR ]->pix16(y, cliprect.min_x);
		uint16_t *dc = &bitmaps[kvideodac_device::BITMAP_COLOR]->pix16(y, cliprect.min_x);
		uint16_t *da = &bitmaps[kvideodac_device::BITMAP_ATTRIBUTES]->pix16(y, cliprect.min_x);

		const uint16_t *const *shadp = m_shadow_layer == LAYER0_ATTR ? &a0 : m_shadow_layer == LAYER1_ATTR ? &a1 : &a2;

		for(int x = cliprect.min_x; x <= cliprect.max_x; x++) {
			uint8_t pri = 0x3f;
			uint16_t color = 0x0000;
			uint16_t cc, ca;
			uint16_t shada = **shadp;
			bool has_pix = false;

			cc = *c0++ & 0x1ff;
			ca = *a0++;
			if((disp & 0x01) && (cc & 0xf)) {
				uint8_t lpri = m_inpri & 1 ? m_pri[0] : ca & 0x3f;
				has_pix = true;
				color = cc | m_cblk[0];
				pri = lpri;
			}

			cc = *c1++ & 0x1ff;
			ca = *a1++;
			if((disp & 0x02) && (cc & 0xf)) {
				uint8_t lpri = m_inpri & 2 ? m_pri[1] : ca & 0x3f;
				if(!has_pix || lpri < pri) {
					has_pix = true;
					color = cc | m_cblk[1];
					pri = lpri;
				}
			}

			cc = *c2++ & 0x1ff;
			ca = *a2++;
			if((disp & 0x04) && (cc & 0xf)) {
				uint8_t lpri = m_inpri & 4 ? m_pri[2] : ca & 0x3f;
				if(!has_pix || lpri < pri) {
					has_pix = true;
					color = cc | m_cblk[2];
					pri = lpri;
				}
			}

			cc = *c3++ & 0x0ff;
			if((disp & 0x08) && (cc & 0xf)) {
				uint8_t lpri = m_pri[3];
				if(!has_pix || lpri < pri) {
					has_pix = true;
					color = cc | m_cblk[3];
					pri = lpri;
				}
			}

			cc = *c4++ & 0x0ff;
			if((disp & 0x10) && (cc & 0xf)) {
				uint8_t lpri = m_pri[4];
				if(!has_pix || lpri < pri) {
					has_pix = true;
					color = cc | m_cblk[4];
					pri = lpri;
				}
			}

			*dc++ = color;

			if(!has_pix)
				*da++ = 0;
			else {
				uint16_t attr = 0x8000;
				if((shada & 1) && pri >= m_sha[0])
					attr |= 1;
				if((shada & 2) && pri >= m_sha[1])
					attr |= 2;
				*da++ = attr;
			}
		}
	}
}
