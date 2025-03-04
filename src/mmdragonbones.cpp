#include "mmdragonbones.h"

//////////////////////////////////////////////////////////////////
//// ResourceLoader

Ref<Resource> ResourceFormatLoaderMMDB::load(const String &path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {

		Ref<MMDragonBonesResource> __p_ref;
		__p_ref.instantiate();
		
		String __str_path_base = path.get_basename();

        __str_path_base = __str_path_base.substr(0, __str_path_base.length() - strlen("_ske"));

        // texture path
        __p_ref->set_def_texture_path(__str_path_base + "_tex.png");

        // loading atlas data
        bool __bret = __p_ref->load_texture_atlas_data(String(__str_path_base + "_tex.json").ascii().get_data());
        ERR_FAIL_COND_V(!__bret, 0);

        // loading bones data
        __bret = __p_ref->load_bones_data(path.ascii().get_data());
        ERR_FAIL_COND_V(!__bret, 0);

		return __p_ref;
	}

void ResourceFormatLoaderMMDB::get_recognized_extensions(List<String> *p_extensions) const
{
	p_extensions->push_back("dbbin");
	p_extensions->push_back("json");
}

bool ResourceFormatLoaderMMDB::handles_type(const String &type) const
{
	return type==StringName("MMDragonBonesResource");
}

String ResourceFormatLoaderMMDB::get_resource_type(const String &p_path) const
{
	String el = p_path.get_extension().to_lower();

    if ((el == "json" || el == "dbbin") && p_path.get_basename().to_lower().ends_with("_ske"))
        return "MMDragonBonesResource";
    return "";
}

//////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//// Resource
MMDragonBonesResource::MMDragonBonesResource()
{
    p_data_texture_atlas = nullptr;
    p_data_bones = nullptr;
}

MMDragonBonesResource::~MMDragonBonesResource()
{
    if(p_data_texture_atlas)
    {
        memfree(p_data_texture_atlas);
        p_data_texture_atlas = nullptr;
    }

    if(p_data_bones)
    {
        memfree(p_data_bones);
        p_data_bones = nullptr;
    }
}

char*  __load_file(const String& _file_path)
{
    Ref<FileAccess> __p_f = FileAccess::open(_file_path, FileAccess::READ);

    ERR_FAIL_COND_V(!__p_f->get_length(), nullptr);

   // mem
    char* __p_data = (char*)memalloc(__p_f->get_length() + 1);
    ERR_FAIL_COND_V(!__p_data, nullptr);

    __p_f->get_buffer((uint8_t *)__p_data, __p_f->get_length());
    __p_data[__p_f->get_length()] = 0x00;

    return __p_data;
}

void       MMDragonBonesResource::set_def_texture_path(const String& _path)
{
    str_default_tex_path = _path;
}

bool       MMDragonBonesResource::load_texture_atlas_data(const String& _path)
{
    p_data_texture_atlas = __load_file(_path);
    ERR_FAIL_COND_V(!p_data_texture_atlas, false);
    return true;
}

bool       MMDragonBonesResource::load_bones_data(const String& _path)
{
    p_data_bones = __load_file(_path);
    ERR_FAIL_COND_V(!p_data_bones, false);
    return true;
}


void 	   MMDragonBonesResource::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("set_def_texture_path", "path"), &MMDragonBonesResource::set_def_texture_path);
	ClassDB::bind_method(D_METHOD("load_texture_atlas_data", "path"), &MMDragonBonesResource::load_texture_atlas_data);
	ClassDB::bind_method(D_METHOD("load_bones_data", "path"), &MMDragonBonesResource::load_bones_data);
}

/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
//// Plugin module
MMDragonBones::MMDragonBones()
{

	p_factory = nullptr;
    m_res = Ref<Resource>();
    str_curr_anim = "[none]";
    p_armature = nullptr;
	m_anim_mode = GDArmatureDisplay::AnimMode::ANIMATION_PROCESS_IDLE;
    f_progress = 0;
    f_speed = 1.f;
    b_processing = false;
    b_active = true;
    b_playing = false;
    b_debug = false;
    c_loop = -1;
    b_inited = false;
    b_try_playing = false;
    b_flip_x = false;
    b_flip_y = false;
    b_inherit_child_material = true;
}

MMDragonBones::~MMDragonBones()
{
    _cleanup();
}

void MMDragonBones::_cleanup()
{
    b_inited = false;

	if (p_armature != nullptr) {
		p_armature->queue_free();
		if (p_armature->get_parent() == this) {
			remove_child(p_armature);
		}
		p_armature = nullptr;
	}

    if (p_factory != nullptr) {
		memfree(p_factory);
		p_factory = nullptr;
	}

    m_res = Ref<Resource>();
}

void MMDragonBones::dispatch_snd_event(const String &_str_type, const EventObject* _p_value)
{
    if(Engine::get_singleton()->is_editor_hint())
		return;

   if(_str_type == EventObject::SOUND_EVENT)
       emit_signal("dragon_anim_snd_event", String(_p_value->animationState->name.c_str()), String(_p_value->name.c_str()));
}

void MMDragonBones::dispatch_event(const String &_str_type, const EventObject* _p_value)
{
    if(Engine::get_singleton()->is_editor_hint())
		return;

    if (_str_type == EventObject::START)
		emit_signal("dragon_anim_start", String(_p_value->animationState->name.c_str()));
	else if (_str_type == EventObject::LOOP_COMPLETE)
		emit_signal("dragon_anim_loop_complete", String(_p_value->animationState->name.c_str()));
	else if (_str_type == EventObject::COMPLETE)
		emit_signal("dragon_anim_complete", String(_p_value->animationState->name.c_str()));
	else if (_str_type == EventObject::FRAME_EVENT) {
		int int_val = 0;
		int float_val = 0;
		const char *string_val = std::string("").c_str();
		UserData* user_data = _p_value->getData();
		Armature* armature = _p_value->getArmature();

		if (user_data != NULL) {
				int_val = _p_value->getData()->getInt(0);
				float_val = _p_value->getData()->getFloat(0);

				if (!user_data->getStrings().empty()) {
					string_val = _p_value->getData()->getString(0).c_str();
				}
		}

		Dictionary dict = Dictionary();

		dict[String("armature")] = String(armature->getName().c_str());
		dict[String("animation")] = String(_p_value->animationState->name.c_str());
		dict[String("event_name")] = String(_p_value->name.c_str());
		dict[String("int")] = int_val;
		dict[String("float")] = float_val;
		dict[String("string")] = string_val;

		emit_signal("dragon_anim_event", dict);
	}
	else if (_str_type == EventObject::FADE_IN)
		emit_signal("dragon_fade_in", String(_p_value->animationState->name.c_str()));
	else if (_str_type == EventObject::FADE_IN_COMPLETE)
		emit_signal("dragon_fade_in_complete", String(_p_value->animationState->name.c_str()));
	else if (_str_type == EventObject::FADE_OUT)
		emit_signal("dragon_fade_out", String(_p_value->animationState->name.c_str()));
	else if (_str_type == EventObject::FADE_OUT_COMPLETE)
		emit_signal("dragon_fade_out_complete", String(_p_value->animationState->name.c_str()));
	
}

void MMDragonBones::set_resource(Ref<MMDragonBonesResource> _p_data)
{
    String __old_texture_path = "";
    if(m_res.is_valid())
        __old_texture_path = m_res->str_default_tex_path;
	else if(_p_data.is_valid())
        __old_texture_path = _p_data->str_default_tex_path;

    if (m_res == _p_data)
		return;

    stop();
    _cleanup();

	p_factory = memnew(GDFactory(this));

    m_res = _p_data;
    if (m_res.is_null())
    {
        m_texture_atlas = Ref<Texture2D>();
        ERR_PRINT("Null resources");
		return;
    }

    ERR_FAIL_COND(!m_res->p_data_texture_atlas);
    ERR_FAIL_COND(!m_res->p_data_bones);

    TextureAtlasData* __p_tad = p_factory->loadTextureAtlasData(m_res->p_data_texture_atlas, nullptr);
    ERR_FAIL_COND(!__p_tad);
    DragonBonesData* __p_dbd = p_factory->loadDragonBonesData(m_res->p_data_bones);
    ERR_FAIL_COND(!__p_dbd);

    // build Armature display
    const std::vector<std::string>& __r_v_m_names = __p_dbd->getArmatureNames();
    ERR_FAIL_COND(!__r_v_m_names.size());

    p_armature = static_cast<GDArmatureDisplay*>(p_factory->buildArmatureDisplay(__r_v_m_names[0].c_str()));
	p_armature->set_name("mm_main_armature");
    // add children armature
    p_armature->p_owner = this;

	// To support non-texture atlas; I'd want to look around here
    if(!m_texture_atlas.is_valid() || __old_texture_path != m_res->str_default_tex_path)
        m_texture_atlas = ResourceLoader::load(m_res->str_default_tex_path);

    // correction for old version of DB tad files (Zero width, height)
    if(m_texture_atlas.is_valid())
    {
        __p_tad->height = m_texture_atlas->get_height();
        __p_tad->width = m_texture_atlas->get_width();
    }

	// update flip
	p_armature->getArmature()->setFlipX(b_flip_x);
	p_armature->getArmature()->setFlipY(b_flip_y);

    p_armature->add_parent_class(b_debug, m_texture_atlas);
    // add main armature
    add_child(p_armature);

	p_armature->force_parent_owned();
    b_inited = true;

    // update color and opacity and blending
    p_armature->update_childs(true, true);

    // update material inheritance
    p_armature->update_material_inheritance(b_inherit_child_material);


    p_armature->getArmature()->advanceTime(0);

    queue_redraw();

	if (!Engine::get_singleton()->is_editor_hint()) {
		_set_process(true);
	}
}

Ref<MMDragonBonesResource> MMDragonBones::get_resource()
{
    return m_res;
}

void MMDragonBones::set_inherit_material(bool _b_enable)
{
    b_inherit_child_material = _b_enable;
    if(p_armature)
	    p_armature->update_material_inheritance(b_inherit_child_material);
}

bool MMDragonBones::is_material_inherited() const
{
    return b_inherit_child_material;
}

void MMDragonBones::fade_in(const String& _name_anim, float _time, int _loop, int _layer, const String& _group, GDArmatureDisplay::AnimFadeOutMode _fade_out_mode)
{
    // setup speed
    p_factory->set_speed(f_speed);
    if(has_anim(_name_anim))
    {
        p_armature->getAnimation()->fadeIn(_name_anim.ascii().get_data(), _time, _loop, _layer, _group.ascii().get_data(), (AnimationFadeOutMode)_fade_out_mode);
        if(!b_playing)
        {
            b_playing = true;
            _set_process(true);
        }
    }
}

void MMDragonBones::fade_out(const String& _name_anim)
{
    if(!b_inited) return;

    if(!p_armature->getAnimation()->isPlaying() 
	|| !p_armature->getAnimation()->hasAnimation(_name_anim.ascii().get_data()))
	return;

    p_armature->getAnimation()->stop(_name_anim.ascii().get_data());

    if(p_armature->getAnimation()->isPlaying())
        return;

    _set_process(false);
    b_playing = false;

    _reset();
}

void MMDragonBones::set_active(bool _b_active)
{
    if (b_active == _b_active)  return;
    b_active = _b_active;
    _set_process(b_processing, true);
}

bool MMDragonBones::is_active() const
{
    return b_active;
}

void MMDragonBones::set_debug(bool _b_debug)
{
    b_debug = _b_debug;
    if(b_inited)
        p_armature->set_debug(b_debug);
}

bool MMDragonBones::is_debug() const
{
    return b_debug;
}

void MMDragonBones::flip_x(bool _b_flip)
{
    b_flip_x = _b_flip;
    if(!p_armature)
        return;
    p_armature->getArmature()->setFlipX(_b_flip);
    p_armature->getArmature()->advanceTime(0);
}

bool MMDragonBones::is_fliped_x() const
{
    return b_flip_x;
}

void MMDragonBones::flip_y(bool _b_flip)
{
    b_flip_y = _b_flip;
    if(!p_armature)
        return;
    p_armature->getArmature()->setFlipY(_b_flip);
    p_armature->getArmature()->advanceTime(0);
}

bool MMDragonBones::is_fliped_y() const
{
    return b_flip_y;
}

void MMDragonBones::set_speed(float _f_speed)
{
    f_speed = _f_speed;
    if(b_inited)
        p_factory->set_speed(_f_speed);
}

float MMDragonBones::get_speed() const
{
	return f_speed;
}

void MMDragonBones::set_animation_process_mode(GDArmatureDisplay::AnimMode _mode)
{
	if (m_anim_mode == _mode)
		return;
	bool __pr = b_processing;
	if (__pr)
		_set_process(false);
	m_anim_mode = _mode;
	if (__pr)
		_set_process(true);
}

GDArmatureDisplay::AnimMode MMDragonBones::get_animation_process_mode() const
{
	return m_anim_mode;
}

void MMDragonBones::_notification(int _what)
{
	switch (_what)
	{
	case NOTIFICATION_ENTER_TREE:
	{
		if (!b_processing)
		{
			set_process(false);
			set_physics_process(false);
		}
	}
	break;

	case NOTIFICATION_READY:
	{
		if (b_playing && b_inited)
			play();
	}
	break;


	case NOTIFICATION_PROCESS:
	{
		if (m_anim_mode == GDArmatureDisplay::AnimMode::ANIMATION_PROCESS_FIXED)
			break;

		if (b_processing)
			p_factory->update(get_process_delta_time());
	}
	break;

	case NOTIFICATION_PHYSICS_PROCESS:
	{

		if (m_anim_mode == GDArmatureDisplay::AnimMode::ANIMATION_PROCESS_IDLE)
			break;

		if (b_processing)
			p_factory->update(get_physics_process_delta_time());
	}
	break;

	case NOTIFICATION_EXIT_TREE:
	{

	}
	break;
	}
}

void    MMDragonBones::_reset()
{
	p_armature->getAnimation()->reset();
}

void MMDragonBones::set_slot_display_index(const String& _slot_name, int _index) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	p_armature->getSlot(_slot_name.ascii().get_data())->setDisplayIndex(_index);
}

void MMDragonBones::set_slot_by_item_name(const String &_slot_name, const String &_item_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	const std::vector<DisplayData *> *rawData = p_armature->getSlot(_slot_name.ascii().get_data())->getRawDisplayDatas();

	// we only want to update the slot if there's a choice
	if (rawData->size() > 1) {
		const char *desired_item = _item_name.ascii().get_data();
		std::string NONE_STRING("none");

		if (NONE_STRING.compare(desired_item) == 0) {
			p_armature->getSlot(_slot_name.ascii().get_data())->setDisplayIndex(-1);
		}

		for (int i = 0; i < rawData->size(); i++) {
			DisplayData *display_data = rawData->at(i);

			if (display_data->name.compare(desired_item) == 0) {
				p_armature->getSlot(_slot_name.ascii().get_data())->setDisplayIndex(i);
				return;
			}
		}
	} else {
		WARN_PRINT("Slot " + _slot_name + " has only 1 item; refusing to set slot");
		return;
	}

	WARN_PRINT("Slot " + _slot_name + " has no item called \"" + _item_name);
}

void MMDragonBones::set_all_slots_by_item_name(const String& _item_name) {
	std::vector<Slot*> slots = p_armature->getArmature()->getSlots();

	for (Slot* slot : slots) {
		set_slot_by_item_name(String(slot->getName().c_str()), _item_name);
	}
}

int MMDragonBones::get_slot_display_index(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return -1;
	}
	return p_armature->getSlot(_slot_name.ascii().get_data())->getDisplayIndex();
}

int MMDragonBones::get_total_items_in_slot(const String& _slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return -1;
	}
	return p_armature->getSlot(_slot_name.ascii().get_data())->getDisplayList().size();
}

bool MMDragonBones::has_slot(const String &_slot_name) const {
	return p_armature->getSlot(_slot_name.ascii().get_data()) != nullptr;
}

void MMDragonBones::cycle_next_item_in_slot(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	int current_slot = get_slot_display_index(_slot_name);
	current_slot++;

	set_slot_display_index(_slot_name, current_slot < get_total_items_in_slot(_slot_name) ? current_slot : -1);
}

void MMDragonBones::cycle_previous_item_in_slot(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	int current_slot = get_slot_display_index(_slot_name);
	current_slot--;

	set_slot_display_index(_slot_name, current_slot >= -1 ? current_slot : get_total_items_in_slot(_slot_name) - 1);
}

Color MMDragonBones::get_slot_display_color_multiplier(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return Color(-1,-1,-1,-1);
	}
	ColorTransform color_transform(p_armature->getSlot(_slot_name.ascii().get_data())->_colorTransform);

	Color return_color;
	return_color.r = color_transform.redMultiplier;
	return_color.g = color_transform.greenMultiplier;
	return_color.b = color_transform.blueMultiplier;
	return_color.a = color_transform.alphaMultiplier;
	return return_color;
}

void MMDragonBones::set_slot_display_color_multiplier(const String &_slot_name, const Color &_color) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	ColorTransform _new_color;
	_new_color.redMultiplier = _color.r;
	_new_color.greenMultiplier = _color.g;
	_new_color.blueMultiplier = _color.b;
	_new_color.alphaMultiplier = _color.a;

	p_armature->getSlot(_slot_name.ascii().get_data())->_setColor(_new_color);
}

void MMDragonBones::play(bool _b_play) {
    b_playing = _b_play;
    if(!_b_play)
    {
        stop();
        return;
    }
	_set_process(true);

    // setup speed
    p_factory->set_speed(f_speed);
    if(has_anim(str_curr_anim))
    {
        p_armature->getAnimation()->play(str_curr_anim.ascii().get_data(), c_loop);
        b_try_playing = false;
    } 

    else // not finded animation stop playing
    {
        b_try_playing = true;
        str_curr_anim  = "[none]";
        stop();
    }
}

void  MMDragonBones::play_from_time(float _f_time)
{
    play();
    if(b_playing)
         p_armature->getAnimation()->gotoAndPlayByTime(str_curr_anim.ascii().get_data(), _f_time, c_loop);
}

void   MMDragonBones::play_from_progress(float _f_progress)
{
    play();
    if(b_playing)
         p_armature->getAnimation()->gotoAndPlayByProgress(str_curr_anim.ascii().get_data(), CLAMP(_f_progress, 0, 1.f), c_loop);
}

void MMDragonBones::play_new_animation_from_progress(const String &_str_anim, int _num_times, float _f_progress) {
	stop_all();
	_set("playback/curr_animation", _str_anim);
	_set("playback/loop", _num_times);
	play(true);

	play_from_progress(_f_progress);
}

void MMDragonBones::play_new_animation_from_time(const String &_str_anim, int _num_times, float _f_time) {
	stop_all();
	_set("playback/curr_animation", _str_anim);
	_set("playback/loop", _num_times);
	play(true);

	play_from_time(_f_time);
}

void MMDragonBones::play_new_animation(const String &_str_anim, int _num_times) {
	stop_all();
	_set("playback/curr_animation", _str_anim);
	_set("playback/loop", _num_times);
	play(true);
}

bool MMDragonBones::has_anim(const String& _str_anim)
{
	return p_armature->has_animation(_str_anim);
}

void MMDragonBones::stop(bool _b_all)
{
    if(!b_inited) return;

    _set_process(false);
    b_playing = false;

    if(p_armature->getAnimation()->isPlaying())
        p_armature->getAnimation()->stop(_b_all?"":str_curr_anim.ascii().get_data());

    _reset();
}

const DragonBonesData *MMDragonBones::get_dragonbones_data() const {
	const DragonBonesData* db_data = this->p_armature->getArmature()->getArmatureData()->getParent();
	return db_data;
}

ArmatureData* MMDragonBones::get_armature_data(const String &_armature_name) {
	std::map<std::string, ArmatureData *>::const_iterator armature_data = get_dragonbones_data()->armatures.find(_armature_name.ascii().get_data());

	if (armature_data == get_dragonbones_data()->armatures.end()) {
		return nullptr;
	}

	return armature_data->second;
}

GDArmatureDisplay *MMDragonBones::get_armature() {
	return p_armature;
}

float MMDragonBones::tell()
{
    if(b_inited && has_anim(str_curr_anim))
    {
        AnimationState* __p_state = p_armature->getAnimation()->getState(str_curr_anim.ascii().get_data());
        if(__p_state)
            return __p_state->getCurrentTime()/__p_state->_duration;
    }
    return 0;
}

void MMDragonBones::seek(float _f_p)
{
    if(b_inited && has_anim(str_curr_anim))
    {
	f_progress = _f_p;
        stop();
	auto __c_p = Math::fmod(_f_p, 1.0f);
	if (__c_p == 0 && _f_p != 0)
		__c_p = 1.0f;
        p_armature->getAnimation()->gotoAndStopByProgress(str_curr_anim.ascii().get_data(), __c_p < 0?1.+__c_p:__c_p);
    }
}

float MMDragonBones::get_progress() const
{
    return f_progress;
}

bool MMDragonBones::is_playing() const
{
    return b_inited && b_playing && p_armature->getAnimation()->isPlaying();
}

String MMDragonBones::get_current_animation() const
{
    if(!b_inited || !p_armature->getAnimation())
        return String("");
    return String(p_armature->getAnimation()->getLastAnimationName().c_str());
}

String MMDragonBones::get_current_animation_on_layer(int _layer) const {

	if (!b_inited || !p_armature->getAnimation())
		return String("");
	std::vector<AnimationState *> states = p_armature->getAnimation()->getStates();

	for (AnimationState* state : states) {
		if (state->layer == _layer) {
			return state->getName().c_str();
		}
	}

	return String("");
}

void MMDragonBones::_set_process(bool _b_process, bool _b_force)
{
    if (b_processing == _b_process && !_b_force)
        return;

    switch (m_anim_mode)
    {

		case GDArmatureDisplay::AnimMode::ANIMATION_PROCESS_FIXED: set_physics_process(_b_process && b_active); break;

		case GDArmatureDisplay::AnimMode::ANIMATION_PROCESS_IDLE: set_process(_b_process && b_active); break;
    }
    b_processing = _b_process;
}

void MMDragonBones::set_texture(const Ref<Texture2D> &_p_texture) {

    if (_p_texture.is_valid()
            && m_texture_atlas.is_valid()
            && (_p_texture == m_texture_atlas
               || m_texture_atlas->get_height() != _p_texture->get_height()
               || m_texture_atlas->get_width()  != _p_texture->get_width()))
        return;

    m_texture_atlas = _p_texture;

    if(p_armature)
    {
        p_armature->update_texture_atlas(m_texture_atlas);
        p_armature->queue_redraw();
    }
}

Ref<Texture2D> MMDragonBones::get_texture() const
{
    return m_texture_atlas;
}

void MMDragonBones::_bind_methods()
{    
    ClassDB::bind_method(D_METHOD("set_texture", "texture"), &MMDragonBones::set_texture);
    ClassDB::bind_method(D_METHOD("get_texture"), &MMDragonBones::get_texture);

    ClassDB::bind_method(D_METHOD("set_resource", "dragonbones"), &MMDragonBones::set_resource);
    ClassDB::bind_method(D_METHOD("get_resource"), &MMDragonBones::get_resource);

    ClassDB::bind_method(D_METHOD("set_inherit_material"), &MMDragonBones::set_inherit_material);
    ClassDB::bind_method(D_METHOD("is_material_inherited"), &MMDragonBones::is_material_inherited);

	ClassDB::bind_method(D_METHOD("fade_in", "anim_name", "time", "loop", "layer", "group", "fade_out_mode"), &MMDragonBones::fade_in);
	ClassDB::bind_method(D_METHOD("fade_out", "anim_name"), &MMDragonBones::fade_out);

	ClassDB::bind_method(D_METHOD("stop"), &MMDragonBones::stop);
	ClassDB::bind_method(D_METHOD("stop_all"), &MMDragonBones::stop_all);
	ClassDB::bind_method(D_METHOD("reset"), &MMDragonBones::_reset);
	ClassDB::bind_method(D_METHOD("has_slot"), &MMDragonBones::has_slot);
	ClassDB::bind_method(D_METHOD("set_slot_by_item_name"), &MMDragonBones::set_slot_by_item_name);
	ClassDB::bind_method(D_METHOD("set_all_slots_by_item_name"), &MMDragonBones::set_all_slots_by_item_name);
	ClassDB::bind_method(D_METHOD("set_slot_display_index"), &MMDragonBones::set_slot_display_index);
	ClassDB::bind_method(D_METHOD("get_slot_display_index"), &MMDragonBones::get_slot_display_index);
	ClassDB::bind_method(D_METHOD("get_total_items_in_slot"), &MMDragonBones::get_total_items_in_slot);
	ClassDB::bind_method(D_METHOD("set_slot_display_color_multiplier"), &MMDragonBones::set_slot_display_color_multiplier);
	ClassDB::bind_method(D_METHOD("get_slot_display_color_multiplier"), &MMDragonBones::get_slot_display_color_multiplier);
	ClassDB::bind_method(D_METHOD("cycle_next_item_in_slot"), &MMDragonBones::cycle_next_item_in_slot);
	ClassDB::bind_method(D_METHOD("cycle_previous_item_in_slot"), &MMDragonBones::cycle_previous_item_in_slot);

    ClassDB::bind_method(D_METHOD("play", "turn_on"), &MMDragonBones::play);
    ClassDB::bind_method(D_METHOD("play_from_time"), &MMDragonBones::play_from_time);
	ClassDB::bind_method(D_METHOD("play_from_progress"), &MMDragonBones::play_from_progress);
	ClassDB::bind_method(D_METHOD("play_new_animation", "anim_name", "loop"), &MMDragonBones::play_new_animation, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_new_animation_from_progress"), &MMDragonBones::play_new_animation_from_progress);
	ClassDB::bind_method(D_METHOD("play_new_animation_from_time"), &MMDragonBones::play_new_animation_from_time);

	
    ClassDB::bind_method(D_METHOD("flip_x", "enable_flip"), &MMDragonBones::flip_x);
	ClassDB::bind_method(D_METHOD("is_fliped_x"), &MMDragonBones::is_fliped_x);
	ClassDB::bind_method(D_METHOD("flip_y", "enable_flip"), &MMDragonBones::flip_y);
	ClassDB::bind_method(D_METHOD("is_fliped_y"), &MMDragonBones::is_fliped_y);

	ClassDB::bind_method(D_METHOD("set_speed", "speed"), &MMDragonBones::set_speed);
	ClassDB::bind_method(D_METHOD("get_speed"), &MMDragonBones::get_speed);

	
    ClassDB::bind_method(D_METHOD("seek", "pos"), &MMDragonBones::seek);
	ClassDB::bind_method(D_METHOD("tell"), &MMDragonBones::tell);
	ClassDB::bind_method(D_METHOD("get_progress"), &MMDragonBones::get_progress);

	ClassDB::bind_method(D_METHOD("has", "name"), &MMDragonBones::has_anim);
	ClassDB::bind_method(D_METHOD("is_playing"), &MMDragonBones::is_playing);

	ClassDB::bind_method(D_METHOD("get_current_animation"), &MMDragonBones::get_current_animation);
	ClassDB::bind_method(D_METHOD("get_current_animation_on_layer"), &MMDragonBones::get_current_animation_on_layer);
	ClassDB::bind_method(D_METHOD("get_armature"), &MMDragonBones::get_armature);

    ClassDB::bind_method(D_METHOD("set_active", "active"), &MMDragonBones::set_active);
    ClassDB::bind_method(D_METHOD("is_active"), &MMDragonBones::is_active);

    ClassDB::bind_method(D_METHOD("set_debug", "debug"), &MMDragonBones::set_debug);
    ClassDB::bind_method(D_METHOD("is_debug"), &MMDragonBones::is_debug);

    ClassDB::bind_method(D_METHOD("set_animation_process_mode","mode"),&MMDragonBones::set_animation_process_mode);
    ClassDB::bind_method(D_METHOD("get_animation_process_mode"),&MMDragonBones::get_animation_process_mode);

	// This is how we set top level properties
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug"), "set_debug", "is_debug");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flippedX"), "flip_x", "is_fliped_x");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flippedY"), "flip_y", "is_fliped_y");

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "MMDragonBonesResource"), "set_resource", "get_resource");

    ADD_PROPERTY(PropertyInfo(Variant::INT, "playback/process_mode", PROPERTY_HINT_ENUM, "Fixed,Idle"), "set_animation_process_mode", "get_animation_process_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "playback/speed", PROPERTY_HINT_RANGE, "-10,10,0.01"), "set_speed", "get_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "playback/progress", PROPERTY_HINT_RANGE, "-100,100,0.010"), "seek", "get_progress");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playback/play"), "play", "is_playing");

    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "childs_use_this_material"), "set_inherit_material", "is_material_inherited");

    ADD_SIGNAL(MethodInfo("dragon_anim_start", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_anim_complete", PropertyInfo(Variant::STRING, "anim")));
	ADD_SIGNAL(MethodInfo("dragon_anim_event", PropertyInfo(Variant::DICTIONARY, "event")));
    ADD_SIGNAL(MethodInfo("dragon_anim_loop_complete", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_anim_snd_event", PropertyInfo(Variant::STRING, "anim"), PropertyInfo(Variant::STRING, "ev")));
    ADD_SIGNAL(MethodInfo("dragon_fade_in", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_fade_in_complete", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_fade_out", PropertyInfo(Variant::STRING, "anim")));
	ADD_SIGNAL(MethodInfo("dragon_fade_out_complete", PropertyInfo(Variant::STRING, "anim")));
}

void MMDragonBones::_get_property_list(List<PropertyInfo>* _p_list) const
{
    List<String> __l_names;

    if (b_inited && p_armature->getAnimation())
    {
        auto __names = p_armature->getAnimation()->getAnimationNames();
        auto __it = __names.cbegin();
        while(__it != __names.cend())
        {
            __l_names.push_back(__it->c_str());
            ++__it;
        }
    }

    __l_names.sort();
    __l_names.push_front("[none]");
    String __str_hint;
    for (List<String>::Element* __p_E = __l_names.front(); __p_E; __p_E = __p_E->next())
    {
       if (__p_E != __l_names.front())
           __str_hint += ",";
           __str_hint += __p_E->get();
    }

    _p_list->push_back(PropertyInfo(Variant::STRING, "playback/curr_animation", PROPERTY_HINT_ENUM, __str_hint));
    _p_list->push_back(PropertyInfo(Variant::INT, "playback/loop", PROPERTY_HINT_RANGE, "-1,100,1"));
}


bool MMDragonBones::_set(const StringName& _str_name, const Variant& _c_r_value)
{
    String name = _str_name;

    if (name == "playback/curr_animation")
    {
		if(str_curr_anim == String(_c_r_value))
			return true;

		str_curr_anim = _c_r_value;
		if (b_inited)
		{
			if (str_curr_anim == "[none]")
				stop();
			else if (has_anim(str_curr_anim))
			{
				if(b_playing || b_try_playing)
					play();
				else
					p_armature->getAnimation()->gotoAndStopByProgress(str_curr_anim.ascii().get_data());
			}
		}
	}
	else if (name == "playback/loop")
	{
		c_loop = _c_r_value;
		if (b_inited && b_playing)
		{
			_reset();
			play();
		}
	}
	else if (name == "playback/progress")
	{
		seek(_c_r_value);
	}

	return true;
}

bool MMDragonBones::_get(const StringName& _str_name, Variant &_r_ret) const
{

    String __name = _str_name;

    if (__name == "playback/curr_animation")
        _r_ret = str_curr_anim;
    else if (__name == "playback/loop")
        _r_ret = c_loop;
    else if (__name == "playback/progress")
        _r_ret = get_progress();
    return true;
}
