// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
#include "emu.h"
#include "k053250.h"

DEFINE_DEVICE_TYPE(K053250, k053250_device, "k053250", "K053250 LVC")

DEVICE_ADDRESS_MAP_START(map, 8, k053250_device)
	AM_RANGE(0x00, 0x00) AM_READWRITE(scrollh_x_r, scrollh_x_w)
	AM_RANGE(0x01, 0x01) AM_READWRITE(scrolll_x_r, scrolll_x_w)
	AM_RANGE(0x02, 0x02) AM_READWRITE(scrollh_y_r, scrollh_y_w)
	AM_RANGE(0x03, 0x03) AM_READWRITE(scrolll_y_r, scrolll_y_w)
	AM_RANGE(0x04, 0x04) AM_READWRITE(control_r,   control_w)
	AM_RANGE(0x06, 0x06) AM_READWRITE(chah_r,      chah_w)
	AM_RANGE(0x07, 0x07) AM_READWRITE(chal_r,      chal_w)
ADDRESS_MAP_END

k053250_device::k053250_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, K053250, tag, owner, clock),
	  m_rom(*this, DEVICE_SELF),
	  m_ram(*this, nullptr)
{
}

void k053250_device::device_start()
{
	m_buffer.resize(0x200*4);

	save_item(NAME(m_buffer));
	save_item(NAME(m_scroll_x));
	save_item(NAME(m_scroll_y));
	save_item(NAME(m_control));
	save_item(NAME(m_cha));
}

void k053250_device::device_reset()
{
	m_scroll_x = 0;
	m_scroll_y = 0;
	m_control = 0;
	m_cha = 0;
}

void k053250_device::set_info(const char *lvcram_tag)
{
	m_ram.set_tag(lvcram_tag);
}

READ8_MEMBER(k053250_device::scrollh_x_r)
{
	return m_scroll_x >> 8;
}

READ8_MEMBER(k053250_device::scrolll_x_r)
{
	return m_scroll_x;
}

WRITE8_MEMBER(k053250_device::scrollh_x_w)
{
	m_scroll_x = (m_scroll_x & 0x00ff) | (data << 8);
}

WRITE8_MEMBER(k053250_device::scrolll_x_w)
{
	m_scroll_x = (m_scroll_x & 0xff00) | data;
}


READ8_MEMBER(k053250_device::scrollh_y_r)
{
	return m_scroll_y >> 8;
}

READ8_MEMBER(k053250_device::scrolll_y_r)
{
	return m_scroll_y;
}

WRITE8_MEMBER(k053250_device::scrollh_y_w)
{
	m_scroll_y = (m_scroll_y & 0x00ff) | (data << 8);
}

WRITE8_MEMBER(k053250_device::scrolll_y_w)
{
	m_scroll_y = (m_scroll_y & 0xff00) | data;
}


READ8_MEMBER(k053250_device::control_r)
{
	return m_control;
}

WRITE8_MEMBER(k053250_device::control_w)
{
	u8 old = m_control;
	m_control = data;

	// start LVC DMA transfer at the falling edge of control register's bit1
	if((old & 0x02) && !(m_control & 0x02))
		memcpy(&m_buffer[0], &m_ram[0], 0x1000);
}


READ8_MEMBER(k053250_device::chah_r)
{
	return m_cha >> 8;
}

READ8_MEMBER(k053250_device::chal_r)
{
	return m_cha;
}

WRITE8_MEMBER(k053250_device::chah_w)
{
	m_cha = (m_cha & 0x00ff) | (data << 8);
}

WRITE8_MEMBER(k053250_device::chal_w)
{
	m_cha = (m_cha & 0xff00) | data;
}

READ16_MEMBER(k053250_device::rom_r)
{
	return m_rom[(m_cha << 11) | (offset >> 1)];
}

void k053250_device::bitmap_update(bitmap_ind16 *bcolor, bitmap_ind16 *battr, const rectangle &cliprect)
{
	logerror("mode %x %c%c%c%c %04x %04x %d-%d %d-%d\n", m_control >> 5,
			 m_control & 0x10 ? 'y' : '-',
			 m_control & 0x08 ? 'x' : '-',
			 m_control & 0x04 ? 'w' : '-',
			 m_control & 0x01 ? 's' : '-',
			 m_cha, m_scroll_y, cliprect.min_x, cliprect.max_x, cliprect.min_y, cliprect.max_y);


	u32 mask = m_rom.length()*2 - 1;

	u16 base_size = 0x100 << ((m_control >> 5) & 3);

	if(base_size == 0x800)
		base_size = 0x400;

	u16 line_mask  = m_control & 0x80 ? 0x1ff : 0x0ff;
	u16 pre_mask  = 0x1ff;
	u16 post_mask = base_size - 1;
	u16 clip_mask = ~post_mask;

	if(m_control & 0x04)
		clip_mask = 0;

	for(int y = cliprect.min_y; y <= cliprect.max_y; y++) {
		u16 *pc = &bcolor->pix16(y, cliprect.min_x);
		u16 *pa = &battr->pix16(y, cliprect.min_x);
		for(int x = cliprect.min_x; x <= cliprect.max_x; x++) {
			u32 c1, c2;
			u32 cx = x - 10 + m_scroll_x;
			if(m_control & 0x08)
				cx = ~cx;

			u32 cy = y + 1 + m_scroll_y;
			if(m_control & 0x10)
				cy = ~cy;

			if(m_control & 0x01) {
				c1 = cy;
				c2 = cx;
			} else {
				c1 = cx;
				c2 = cy;
			}


			u32 off = (c1 & line_mask) << 2;

			u16 prco = m_buffer[off++];
			u16 roff = m_buffer[off++];
			u16 zoom = m_buffer[off++];
			u16 posx = (c2 + m_buffer[off]) & pre_mask;
			posx = (posx * zoom) >> 6;

			if(posx & clip_mask) {
				roff = posx = 0;
				prco = 0x3f << 5;
			}

			u32 output = ((roff << 8) & ~post_mask) | (posx & post_mask);
			u8 pix = m_rom[(output >> 1) & mask];
			if(output & 1)
				pix &= 0xf;
			else
				pix >>= 4;
			
			*pa++ = prco >> 5;
			*pc++ = ((prco & 0x1f) << 4) | pix;
		}
	}
}
