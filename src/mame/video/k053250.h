// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
#ifndef MAME_VIDEO_K053250_H
#define MAME_VIDEO_K053250_H

#pragma once

//
//  Konami 053250 road generator
//

/*


Register map:

     7   6   5   4   3   2   1   0
+0   .   .   .   .   .   .   -----
+1   ---------scroll x------------
+2   .   .   .   .   .   .   -----
+3   ---------scroll y------------
+4 swd   -dwd-  fy  fx  sc dma  sw
+5   .   .   .   .   .   .   .   .
+6   .   .   .   .   .   .   -----
+7   -----------cha---------------


Ram block entry:

+0   priority (6 bits) / color (5 bits)
+2   offset, in 256-pixels units
+4   zoom
+6   scroll

*/

#define MCFG_K053250_ADD(_tag, _lvcram_tag) \
	MCFG_DEVICE_ADD(_tag, K053250, 0) \
	downcast<k053250_device *>(device)->set_info(_lvcram_tag);

class k053250_device :  public device_t
{
public:
	k053250_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	void set_info(const char *lvcram_tag);

	DECLARE_ADDRESS_MAP(map, 8);

	DECLARE_READ16_MEMBER(rom_r);

	void bitmap_update(bitmap_ind16 *bcolor, bitmap_ind16 *battr, const rectangle &cliprect);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	// internal state
	required_region_ptr<u8> m_rom;
	required_shared_ptr<u16> m_ram;
	std::vector<u16> m_buffer;
	u16 m_scroll_x, m_scroll_y, m_cha;
	u8 m_control;

	DECLARE_READ8_MEMBER (scrollh_x_r);
	DECLARE_READ8_MEMBER (scrolll_x_r);
	DECLARE_WRITE8_MEMBER(scrollh_x_w);
	DECLARE_WRITE8_MEMBER(scrolll_x_w);
	DECLARE_READ8_MEMBER (scrollh_y_r);
	DECLARE_READ8_MEMBER (scrolll_y_r);
	DECLARE_WRITE8_MEMBER(scrollh_y_w);
	DECLARE_WRITE8_MEMBER(scrolll_y_w);
	DECLARE_READ8_MEMBER (control_r);
	DECLARE_WRITE8_MEMBER(control_w);
	DECLARE_READ8_MEMBER (chah_r);
	DECLARE_READ8_MEMBER (chal_r);
	DECLARE_WRITE8_MEMBER(chah_w);
	DECLARE_WRITE8_MEMBER(chal_w);
};

DECLARE_DEVICE_TYPE(K053250, k053250_device)

#endif // MAME_VIDEO_K053250_H
