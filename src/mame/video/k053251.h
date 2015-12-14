// license:BSD-3-Clause
// copyright-holders: Olivier Galibert
#ifndef MAME_VIDEO_K053251_H
#define MAME_VIDEO_K053251_H

#define MCFG_K053251_ADD(_tag, _shadow_layer)		\
	MCFG_DEVICE_ADD(_tag, K053251, 0) \
	downcast<k053251_device *>(device)->set_shadow_layer(_shadow_layer);

#define MCFG_K053251_SET_INIT_CB(_tag, _class, _method)					\
	downcast<k053251_device *>(device)->set_init_cb(k053251_device::init_cb(&_class::_method, #_class "::" #_method, _tag, (_class *)nullptr));

#define MCFG_K053251_SET_UPDATE_CB(_tag, _class, _method)				\
	downcast<k053251_device *>(device)->set_update_cb(k053251_device::update_cb(&_class::_method, #_class "::" #_method, _tag, (_class *)nullptr));

class k053251_device : public device_t
{
public:
	enum {
		LAYER0_COLOR,
		LAYER1_COLOR,
		LAYER2_COLOR,
		LAYER3_COLOR,
		LAYER4_COLOR,
		LAYER0_ATTR,
		LAYER1_ATTR,
		LAYER2_ATTR,
		BITMAP_COUNT
	};
		
	typedef device_delegate<void (bitmap_ind16 **)> init_cb;
	typedef device_delegate<void (bitmap_ind16 **, const rectangle &)> update_cb;

	k053251_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void set_shadow_layer(int layer);
	void set_init_cb(init_cb _cb) { m_init_cb = _cb; }
	void set_update_cb(update_cb _cb) { m_update_cb = _cb; }

	DECLARE_WRITE8_MEMBER(pri_w);
	DECLARE_WRITE8_MEMBER(sha_w);
	DECLARE_WRITE8_MEMBER(cblk_w);
	DECLARE_WRITE8_MEMBER(inpri_w);
	DECLARE_WRITE8_MEMBER(extsha_w);

	DECLARE_ADDRESS_MAP(map, 8);

	void bitmap_update(bitmap_ind16 **bitmaps, const rectangle &cliprect);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	init_cb m_init_cb;
	update_cb m_update_cb;

	bitmap_ind16 *m_bitmaps[BITMAP_COUNT];

	int m_shadow_layer;

	uint8_t m_pri[5], m_sha[2];
	uint8_t m_inpri, m_extsha;
	uint16_t m_cblk[5];
};

DECLARE_DEVICE_TYPE(K053251, k053251_device)


#endif // MAME_VIDEO_K053251_H
