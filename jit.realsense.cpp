#include <librealsense2/rs.hpp>
#include "jit.realsense.hpp"
#include "max_utils.hpp"

static rs2_stream stream_from_long(long stream) {
	return static_cast<rs2_stream>(stream);
}

static
rs2_format best_format(rs2_stream other)
{
	using namespace std;
	switch(other)
	{
		case RS2_STREAM_DEPTH: return RS2_FORMAT_Z16;
		case RS2_STREAM_INFRARED: return RS2_FORMAT_Y8;
		case RS2_STREAM_COLOR: return RS2_FORMAT_RGB8;
		default:
			break;
	}
	throw std::runtime_error("Invalid stream requested");
}


struct jit_rs_streaminfo
{
	long stream = RS2_STREAM_INFRARED;
	long stream_index = 1;
	long rate = 60;
	long dimensions_size = 2; // do not remove, Max uses it.
	std::array<long, 2> dimensions{{640, 480}};

	friend bool operator!=(const jit_rs_streaminfo& lhs, const jit_rs_streaminfo& rhs)
	{
		return lhs.stream != rhs.stream
				|| lhs.stream_index != rhs.stream_index
				|| lhs.rate != rhs.rate
				|| lhs.dimensions != rhs.dimensions;
	}
	friend bool operator==(const jit_rs_streaminfo& lhs, const jit_rs_streaminfo& rhs)
	{
		return !(lhs != rhs);
	}
};

// Our Jitter object instance data
struct t_jit_realsense
{
	t_object	ob;
	rs2::device dev;
	rs2::config cfg;
	rs2::pipeline pipe;
	rs2::pipeline_profile profile;
	bool streaming{false};

	long device = 0;
	long out_count = 1;

	std::array<jit_rs_streaminfo, jit_realsense_max_num_outlets> outputs;

	std::size_t device_cache = 0;
	std::array<jit_rs_streaminfo, jit_realsense_max_num_outlets> outputs_cache;

	void construct()
	{
		device = 0;
		out_count = 1;
		outputs = std::array<jit_rs_streaminfo, jit_realsense_max_num_outlets>{};

		device_cache = 0;
		outputs_cache = outputs;
	}

	void rebuild()
	try
	{
		auto& ctx = t_jit_realsense::context();
		// First cleanup if device is changing
		cleanup();
		dev = nullptr;

		// Try to get the new device
		auto devs = ctx.query_devices();
		auto n_dev = devs.size();
		post("There are %d connected RealSense devices.\n", n_dev);

		if(n_dev <= device)
		{
			post("Device %d is not connected.", device);
			return;
		}

		dev = devs[device];


		post("\nUsing device %d, an %s\n", device, dev.get_info(rs2_camera_info::RS2_CAMERA_INFO_NAME));
		post("		Serial number: %s\n", dev.get_info(rs2_camera_info::RS2_CAMERA_INFO_SERIAL_NUMBER));
		post("		Firmware version: %s\n", dev.get_info(rs2_camera_info::RS2_CAMERA_INFO_FIRMWARE_VERSION));

		rebuild_streams();

		device_cache = device;
	}
	catch(const std::exception & e)
	{
		error("realsense: %s\n", e.what());
		if(dev)
			dev = nullptr;
	}

	void rebuild_streams()
	try
	{
		cleanup();

		// First enable all native streams
		for(long i = 0; i < out_count; i++)
		{
			const jit_rs_streaminfo& out = outputs[(std::size_t)i];
			auto format = best_format(stream_from_long(out.stream));
			//auto si = out.stream == RS2_STREAM_INFRARED ? std::clamp(out.stream_index, 1, 2) : -1;

			cfg.enable_device(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
			cfg.enable_stream(stream_from_long(out.stream), /*si,*/ out.dimensions[0], out.dimensions[1], format, out.rate);
		}

		outputs_cache = outputs;
		if(out_count > 0)
		{
			profile = pipe.start(cfg);
			streaming = true;
		}
	}
	catch(const std::exception & e)
	{
		error("realsense: %s\n", e.what());
	}

	static rs2::context& context()
	{
		static rs2::context ctx;
		return ctx;
	}

	void cleanup()
	try
	{
		if(!dev)
			return;

		if(streaming)
			pipe.stop();
		profile = {};
		streaming = false;

		cfg.disable_all_streams();
	}
	catch(const std::exception & e)
	{
		error("realsense: %s\n", e.what());
	}

	static inline t_class* max_class{};
};


t_jit_realsense *jit_realsense_new(long outcount)
{
	auto obj = jit_new<t_jit_realsense>(t_jit_realsense::max_class);
	obj->out_count = outcount;
	obj->rebuild();
	return obj;
}

void jit_realsense_free(t_jit_realsense *x)
{
	if(x->streaming)
	{
		x->pipe.stop();
	}
}

static
bool compare_matrix_info(t_jit_matrix_info& current, t_jit_matrix_info expected)
{
	bool res = true;
	res &= current.planecount == expected.planecount;
	res &= current.dimcount == expected.dimcount;
	res &= current.type == expected.type;
	if(!res)
		return false;

	for(int i = 0; i < current.dimcount; i++)
	{
		res &= current.dim[i] == expected.dim[i];
	}

	return res;
}

static
char* make_n_plane_matrix(
		t_jit_matrix_info& out_minfo,
		void* out_matrix,
		const rs2::video_stream_profile& intrin,
		int n_plane,
		t_symbol* type)
{
	// First fill a matrix_info with the values we want
	t_jit_matrix_info expected = out_minfo;

	expected.planecount = n_plane;
	expected.dimcount = 2;
	expected.dim[0] = intrin.width();
	expected.dim[1] = intrin.height();
	expected.type = type;
	
	// not sure if this is true yet
	expected.flags = JIT_MATRIX_DATA_PACK_TIGHT;

	// Compare this matrix with the one in out_minfo
	if(!compare_matrix_info(out_minfo, expected))
	{
		// Change the matrix if it is different
		
	// use setinfo_ex to preserve the flags
		jit_object_method(out_matrix, _jit_sym_setinfo_ex, &expected);
		auto old = expected;

		jit_object_method(out_matrix, _jit_sym_getinfo, &expected);
		//post("%d %d", old.dim[0], old.dim[1]);
	}

	// Return a pointer to the data
	char* out_bp{};
	jit_object_method(out_matrix, _jit_sym_getdata, &out_bp);
	return out_bp;
}

static
int num_planes_from_stream(rs2_stream other)
{
	using namespace std;
	switch(other)
	{
		case RS2_STREAM_DEPTH: return 1;
		case RS2_STREAM_COLOR: return 3;
		case RS2_STREAM_INFRARED: return 1;
		default:
			break;
	}
	throw std::runtime_error("Invalid stream");
}

static
int num_planes_from_format(rs2_format format)
{
	switch(format)
	{
		case RS2_FORMAT_ANY:
			throw std::logic_error{"any unhandled"};
		case RS2_FORMAT_Z16:
		case RS2_FORMAT_DISPARITY16:
		case RS2_FORMAT_Y8:
		case RS2_FORMAT_Y16:
		case RS2_FORMAT_YUYV:
			return 1;
		case RS2_FORMAT_RGB8:
		case RS2_FORMAT_BGR8:
		case RS2_FORMAT_XYZ32F:
			return 3;
		case RS2_FORMAT_RGBA8:
		case RS2_FORMAT_BGRA8:
		case RS2_FORMAT_RAW10:
			return 4;
	}
	throw std::logic_error{"num_planes_from_format unhandled"};
}

static
t_symbol * symbol_from_format(rs2_format format)
{
	switch(format)
	{
		case RS2_FORMAT_Z16:
		case RS2_FORMAT_Y16:
		case RS2_FORMAT_DISPARITY16:
			return _jit_sym_long;
		case RS2_FORMAT_Y8:
		case RS2_FORMAT_RGB8:
		case RS2_FORMAT_BGR8:
		case RS2_FORMAT_RGBA8:
		case RS2_FORMAT_BGRA8:
		case RS2_FORMAT_YUYV:
			return _jit_sym_char;
		case RS2_FORMAT_XYZ32F:
			return _jit_sym_float32;
			// Weird cases :
		case RS2_FORMAT_RAW10:
		case RS2_FORMAT_ANY:
			throw std::logic_error{"raw10, any unhandled"};
	}
	throw std::logic_error{"symbol_from_format unhandled"};
}

static
t_symbol * symbol_from_stream(rs2_stream stream)
{
	switch(stream)
	{
		case RS2_STREAM_DEPTH:
			return _jit_sym_long;

		case RS2_STREAM_INFRARED:
		case RS2_STREAM_COLOR:
			return _jit_sym_char;
	}
	throw std::logic_error{"symbol_from_stream unhandled"};
}

template<rs2_format>
struct copier;

// 16 bit case
template<>
struct copier<RS2_FORMAT_Z16>
{
	void operator()(int size, const rs2::frame& rs_matrix, char* max_matrix)
	{
		const auto image = (const uint16_t *)(rs_matrix.get_data());
		auto matrix_out = (t_int32*)(max_matrix);

		std::copy(image, image + size, matrix_out);
	}
};

// 8 bit case
template<>
struct copier<RS2_FORMAT_Y8>
{
	void operator()(int size, const rs2::frame& rs_matrix, char* max_matrix)
	{
		const auto image = (const uint8_t *)(rs_matrix.get_data());
		auto matrix_out = (char*)(max_matrix);

		std::copy(image, image + size, matrix_out);
	}
};

static
void do_copy(rs2_format str, int size, const rs2::frame& rs_matrix, char* max_matrix)
{
	switch(str)
	{
		case RS2_FORMAT_Z16: return copier<RS2_FORMAT_Z16>{}(size, rs_matrix, max_matrix);
		case RS2_FORMAT_Y8: return copier<RS2_FORMAT_Y8>{}(size, rs_matrix, max_matrix);
		case RS2_FORMAT_RGB8: return copier<RS2_FORMAT_Y8>{}(size, rs_matrix, max_matrix);
	}
}

static
void compute_output(t_jit_realsense *x, void *matrix, const jit_rs_streaminfo& info, const rs2::frameset& frames)
{
	const auto num_planes = num_planes_from_stream(stream_from_long(info.stream));
	const auto sym = symbol_from_stream(stream_from_long(info.stream));
	const rs2_stream stream = stream_from_long(info.stream);

	auto lock = jit_object_method(matrix, _jit_sym_lock, 1);

	t_jit_matrix_info out_minfo{};
	jit_object_method(matrix, _jit_sym_getinfo, &out_minfo);

	// Get the realsense informations and compare them
	// with the current matrix.
	const auto stream_profile = x->profile.get_stream(stream).as<rs2::video_stream_profile>();
	char* out_bp = make_n_plane_matrix(out_minfo, matrix, stream_profile,
																		 num_planes,
																		 sym);

	// Copy the data in the Max Matrix
	int size = stream_profile.height() * stream_profile.width() * num_planes;
	//post("FORMAT: %d", info.format);
	do_copy(best_format(stream_from_long(info.stream)), size, frames.first(stream), out_bp);

	jit_object_method(matrix, _jit_sym_lock, lock);
}

static
t_jit_err jit_realsense_matrix_calc(t_jit_realsense* x, void *, void *outputs)
try
{
	// Get and check the data.
	if(!x || !x->dev)
	{
		error("No device");
		return JIT_ERR_INVALID_PTR;
	}

	if(x->device != (long)x->device_cache)
	{
		x->rebuild();
	}
	else if(x->outputs != x->outputs_cache)
	{
		x->rebuild_streams();
	}

	if(x->out_count == 0)
		return JIT_ERR_NONE;

	// Fetch new frame from the realsense
	auto frameset = x->pipe.wait_for_frames();

	for(int i = 0; i < (int)x->out_count; i++)
	{
		if (auto matrix = jit_object_method(outputs, _jit_sym_getindex, i))
		{
			compute_output(x, matrix, x->outputs[i], frameset);
		}
	}

	return JIT_ERR_NONE;
}
catch(const std::exception & e)
{
	error("%s\n", e.what());
	x->cleanup();
	return JIT_ERR_GENERIC;
}

t_jit_err jit_realsense_init()
{
	t_jit_object	*mop;

	t_jit_realsense::max_class = (t_class*)jit_class_new("jit_realsense", (method)jit_realsense_new, (method)jit_realsense_free, sizeof(t_jit_realsense), A_DEFLONG, 0);

	// add matrix operator (mop)
	mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 0, -1); //no matrix inputs, and variable number of outputs
	jit_class_addadornment(t_jit_realsense::max_class, mop);

	// add method(s)
	jit_class_addmethod(t_jit_realsense::max_class, (method)jit_realsense_matrix_calc, "matrix_calc", A_CANT, 0);

	// Add attributes :

	add_attribute("rs_device", &t_jit_realsense::device);
	add_attribute_flags("rs_out_count", &t_jit_realsense::out_count, JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_OPAQUE_USER);

	add_output_attribute<t_jit_realsense>("rs_stream", 0, &jit_rs_streaminfo::stream);
	CLASS_ATTR_LABEL(t_jit_realsense::max_class, "rs_stream", 0, "Out 1 Stream");
	class_attr_enumindex(t_jit_realsense::max_class, "rs_stream",
											 "Any", "Depth", "Color", "Infrared");

	add_output_attribute<t_jit_realsense>("rs_rate", 0, &jit_rs_streaminfo::rate);
	CLASS_ATTR_LABEL(t_jit_realsense::max_class, "rs_rate", 0, "Out 1 Rate");
	CLASS_ATTR_FILTER_CLIP(t_jit_realsense::max_class, "rs_rate", 0, 120);

	add_output_attribute<t_jit_realsense>("rs_index", 0, &jit_rs_streaminfo::stream_index);
	CLASS_ATTR_LABEL(t_jit_realsense::max_class, "rs_index", 0, "Out 1 Index");
	CLASS_ATTR_FILTER_CLIP(t_jit_realsense::max_class, "rs_index", 1, 2);

	add_array_output_attribute<t_jit_realsense>("rs_dim", 0, &jit_rs_streaminfo::dimensions);
	CLASS_ATTR_LABEL(t_jit_realsense::max_class, "rs_dim", 0, "Out 1 Dims");

	/*for(int i = 1; i < jit_realsense_num_outlets; i++)
	{
		const std::string num_str = std::to_string(i + 1);
		const std::string out_str = "out" + num_str;
		const std::string out_maj_str = "Out" + num_str + " ";

		{
			auto attr = out_str + "_rs_stream";
			auto pretty = out_maj_str + "Stream";
			add_output_attribute<t_jit_realsense>(attr, i, &jit_rs_streaminfo::stream);
			CLASS_ATTR_LABEL(t_jit_realsense::max_class, attr.c_str(), 0, pretty.c_str());
			class_attr_enumindex(t_jit_realsense::max_class, attr,
													 "Any", "Depth", "Color", "Infrared");
		}

		{
			auto attr = out_str + "_rs_rate";
			auto pretty = out_maj_str + "Rate";
			add_output_attribute<t_jit_realsense>(attr, i, &jit_rs_streaminfo::rate);
			CLASS_ATTR_LABEL(t_jit_realsense::max_class, attr.c_str(), 0, pretty.c_str());
			CLASS_ATTR_FILTER_CLIP(t_jit_realsense::max_class, "rs_rate", 0, 120);
		}

		{
			auto attr = out_str + "_rs_index";
			auto pretty = out_maj_str + "Index";
			add_output_attribute<t_jit_realsense>(attr, i, &jit_rs_streaminfo::stream_index);
			CLASS_ATTR_LABEL(t_jit_realsense::max_class, attr.c_str(), 0, pretty.c_str());
			CLASS_ATTR_FILTER_CLIP(t_jit_realsense::max_class, "rs_index", 0, 2);
		}

		{
			auto attr = out_str + "_rs_dim";
			auto pretty = out_maj_str + "Dims";
			add_array_output_attribute<t_jit_realsense>(attr, i, &jit_rs_streaminfo::dimensions);
			CLASS_ATTR_LABEL(t_jit_realsense::max_class, attr.c_str(), 0, pretty.c_str());
		}
	}*/


	// finalize class
	jit_class_register(t_jit_realsense::max_class);
	return JIT_ERR_NONE;
}
