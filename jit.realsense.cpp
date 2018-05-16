#define _HAS_AUTO_PTR_ETC 1
#include <librealsense2/rs.hpp>
#include <vector>
#include <array>
#include <cstdint>
#include "jit.common.h"

// To be able to iterate over rs::stream

static const constexpr int jit_realsense_num_outlets = 6;

std::pair<rs2_stream, rs2_stream> native_streams(rs2_stream other)
{
    using namespace std;
    switch(other)
    {
        case RS2_STREAM_DEPTH: return make_pair(RS2_STREAM_DEPTH, RS2_STREAM_DEPTH);
        case RS2_STREAM_COLOR: return make_pair(RS2_STREAM_COLOR, RS2_STREAM_COLOR);
        case RS2_STREAM_INFRARED: return make_pair(RS2_STREAM_INFRARED, RS2_STREAM_INFRARED);
    default:
        break;
    }
    throw std::runtime_error("Invalid stream requested");
}


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
        rs2_stream stream = RS2_STREAM_INFRARED;
        rs2_format format = RS2_FORMAT_Z16;
        long rate = 60;
        long dimensions_size = 2;
        std::array<long, 2> dimensions{{640, 480}};

        friend bool operator!=(const jit_rs_streaminfo& lhs, const jit_rs_streaminfo& rhs)
        {
            return lhs.stream != rhs.stream
                 || lhs.format != rhs.format
                 || lhs.rate != rhs.rate
                 || lhs.dimensions != rhs.dimensions;
        }
        friend bool operator==(const jit_rs_streaminfo& lhs, const jit_rs_streaminfo& rhs)
        {
            return !(lhs != rhs);
        }
};

// Our Jitter object instance data
typedef struct _jit_realsense {
        t_object	ob;
        rs2::device dev;
        rs2::config cfg;
        rs2::pipeline pipe;
        rs2::pipeline_profile profile;
        bool streaming{false};

        std::uint32_t device = 0;
        std::size_t out_count = 1;

        std::array<jit_rs_streaminfo, jit_realsense_num_outlets> outputs;

        std::size_t device_cache = 0;
        std::size_t out_count_cache = 1;
        std::array<jit_rs_streaminfo, jit_realsense_num_outlets> outputs_cache;

        void construct()
        {
            device = 0;
            out_count = 1;
            outputs = std::array<jit_rs_streaminfo, jit_realsense_num_outlets>{};

            device_cache = 0;
            out_count_cache = 1;
            outputs_cache = outputs;
        }

        void rebuild()
        try
        {
            auto& ctx = _jit_realsense::context();
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
            post("    Serial number: %s\n", dev.get_info(rs2_camera_info::RS2_CAMERA_INFO_SERIAL_NUMBER));
            post("    Firmware version: %s\n", dev.get_info(rs2_camera_info::RS2_CAMERA_INFO_FIRMWARE_VERSION));

            rebuild_streams();

            device_cache = device;
            out_count_cache = out_count;
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
            for(std::size_t i = 0; i < out_count; i++)
            {
                jit_rs_streaminfo& out = outputs[(std::size_t)i];
                auto streams = native_streams(out.stream);
                if(streams.first == streams.second && streams.first == out.stream) // Native stream
                {
                    auto format = best_format(streams.first);
                    cfg.enable_stream(out.stream, out.dimensions[0], out.dimensions[1], format, out.rate);
                }
            }

            // Then enable aligned streams, since they require native streams.
            // If a native stream is missing, it is created with decent defaults.
            for(std::size_t i = 0; i < out_count; i++)
            {
                jit_rs_streaminfo& out = outputs[i];
                auto streams = native_streams(out.stream);
                if(streams.first != streams.second ||  streams.first != out.stream) // Aligned stream
                {
                    //if(!cfg.is_stream_enabled(streams.first))
                        cfg.enable_stream(streams.first);
                    //if(!dev->is_stream_enabled(streams.second))
                        cfg.enable_stream(streams.second);
                }
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

} t_jit_realsense;

// prototypes
BEGIN_USING_C_LINKAGE
t_jit_err		jit_realsense_init				(void);
t_jit_realsense	*jit_realsense_new				(void);
void			jit_realsense_free				(t_jit_realsense *x);
t_jit_err		jit_realsense_matrix_calc		(t_jit_realsense* x, void *, void *outputs);
END_USING_C_LINKAGE


// globals
static t_class* s_jit_realsense_class = NULL;
template <typename T, typename U>
intptr_t get_offset(T U::*member)
{
    return reinterpret_cast<intptr_t>(&(((U*) nullptr)->*member));
}
template <typename T, typename U>
intptr_t get_offset(T U::*member, int N)
{
    return reinterpret_cast<intptr_t>(&((((U*) nullptr)->*member)[N]));
}


template <typename T>
static void add_attribute(std::string name, T t_jit_realsense::*member)
{
    const auto flags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    jit_class_addattr(s_jit_realsense_class,
                      (t_jit_object*) jit_object_new(
                          _jit_sym_jit_attr_offset,
                          name.c_str(),
                          _jit_sym_long,
                          flags,
                          (method)nullptr,
                          (method)nullptr,
                          get_offset(member)));
}

template<typename T>
static void add_output_attribute(std::string name, int num, T jit_rs_streaminfo::*member)
{
    const auto flags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    const auto outputs_offset = get_offset(&t_jit_realsense::outputs, num);

    jit_class_addattr(s_jit_realsense_class,
                      (t_jit_object*) jit_object_new(
                          _jit_sym_jit_attr_offset,
                          name.c_str(),
                          _jit_sym_long,
                          flags,
                          (method)nullptr,
                          (method)nullptr,
                          outputs_offset + get_offset(member)));
}
template<typename T>
static void add_array_output_attribute(std::string name, int num, T jit_rs_streaminfo::*member)
{
    const auto flags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    const auto outputs_offset = get_offset(&t_jit_realsense::outputs, num);

    jit_class_addattr(s_jit_realsense_class,
                      (t_jit_object*) jit_object_new(
                          _jit_sym_jit_attr_offset_array,
                          name.c_str(),
                          _jit_sym_long,
                          2,
                          flags,
                          (method)nullptr,
                          (method)nullptr,
                          outputs_offset + get_offset(member) - sizeof(long),
                          outputs_offset + get_offset(member)));
}

/************************************************************************************/

void class_attr_enumindex_rec(t_atom*)
{

}
template<typename Arg, typename... Args>
void class_attr_enumindex_rec(t_atom* aaa, Arg&& arg, Args&&... args)
{
    atom_setsym(aaa, gensym_tr(arg));
    class_attr_enumindex_rec(aaa + 1, std::forward<Args>(args)...);
}

template<typename... Args>
void class_attr_enumindex(t_class* theclass, std::string attrname, Args&&... args)
{
    constexpr int num = sizeof...(Args);
    t_atom aaa[num];
    CLASS_ATTR_STYLE(theclass, attrname.c_str(), 0, "enumindex");
    class_attr_enumindex_rec(aaa, std::forward<Args>(args)...);
    CLASS_ATTR_ATTR_ATOMS(theclass, attrname.c_str(), "enumvals", USESYM(atom), 1, num, aaa);
}

t_jit_err jit_realsense_init(void)
{
    t_jit_object	*mop;

    s_jit_realsense_class = (t_class*)jit_class_new("jit_realsense", (method)jit_realsense_new, (method)jit_realsense_free, sizeof(t_jit_realsense), 0);

    // add matrix operator (mop)
    mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 0, jit_realsense_num_outlets); // args are  num inputs and num outputs
    jit_class_addadornment(s_jit_realsense_class, mop);

    // add method(s)
    jit_class_addmethod(s_jit_realsense_class, (method)jit_realsense_matrix_calc, "matrix_calc", A_CANT, 0);

    // Add attributes :

    add_attribute("rs_device", &t_jit_realsense::device);
    add_attribute("rs_out_count", &t_jit_realsense::out_count);

    add_output_attribute("rs_stream", 0, &jit_rs_streaminfo::stream);
    CLASS_ATTR_LABEL(s_jit_realsense_class, "rs_stream", 0, "Out 1 Stream");
    class_attr_enumindex(s_jit_realsense_class, "rs_stream",
                         "Any", "Depth", "Color", "Infrared", "FishEye");
/*
    add_output_attribute("rs_format", 0, &jit_rs_streaminfo::format);
    CLASS_ATTR_LABEL(s_jit_realsense_class, "rs_format", 0, "Out 1 Format");
    class_attr_enumindex(s_jit_realsense_class, "rs_format", "any", "z16", "disparity16", "xyz32f", "yuyv", "rgb8", "bgr8", "rgba8", "bgra8", "y8", "y16", "raw10");
*/
    add_output_attribute("rs_rate", 0, &jit_rs_streaminfo::rate);
    CLASS_ATTR_LABEL(s_jit_realsense_class, "rs_rate", 0, "Out 1 Rate");
    CLASS_ATTR_FILTER_CLIP(s_jit_realsense_class, "rs_rate", 0, 120);

    add_array_output_attribute("rs_dim", 0, &jit_rs_streaminfo::dimensions);
    CLASS_ATTR_LABEL(s_jit_realsense_class, "rs_dim", 0, "Out 1 Dims");

    for(int i = 1; i < jit_realsense_num_outlets; i++)
    {
        std::string num_str = std::to_string(i + 1);
        std::string out_str = "out" + num_str;
        std::string out_maj_str = "Out" + num_str + " ";

        {
            auto attr = out_str + "_rs_stream";
            auto pretty = out_maj_str + "Stream";
            add_output_attribute(attr, i, &jit_rs_streaminfo::stream);
            CLASS_ATTR_LABEL(s_jit_realsense_class, attr.c_str(), 0, pretty.c_str());
            class_attr_enumindex(s_jit_realsense_class, attr,
                                 "Any", "Depth", "Color", "Infrared", "FishEye");
        }
/*
        {
            auto attr = out_str + "_rs_format";
            auto pretty = out_maj_str + "Format";
            add_output_attribute(attr, i, &jit_rs_streaminfo::format);
            CLASS_ATTR_LABEL(s_jit_realsense_class, attr.c_str(), 0, pretty.c_str());
            class_attr_enumindex(s_jit_realsense_class, attr, "any", "z16", "disparity16", "xyz32f", "yuyv", "rgb8", "bgr8", "rgba8", "bgra8", "y8", "y16", "raw10");
        }
*/

        {
            auto attr = out_str + "_rs_rate";
            auto pretty = out_maj_str + "Rate";
            add_output_attribute(attr, i, &jit_rs_streaminfo::rate);
            CLASS_ATTR_LABEL(s_jit_realsense_class, attr.c_str(), 0, pretty.c_str());
            CLASS_ATTR_FILTER_CLIP(s_jit_realsense_class, "rs_rate", 0, 120);
        }

        {
            auto attr = out_str + "_rs_dim";
            auto pretty = out_maj_str + "Dims";
            add_array_output_attribute(attr, i, &jit_rs_streaminfo::dimensions);
            CLASS_ATTR_LABEL(s_jit_realsense_class, attr.c_str(), 0, pretty.c_str());
        }
    }


    // finalize class
    jit_class_register(s_jit_realsense_class);
    return JIT_ERR_NONE;
}

template<typename T, typename... Args>
T* jit_new(t_class* cls, Args&&... args)
{
  auto obj = jit_object_alloc(cls);
  if(obj)
  {
    t_object tmp;
    memcpy(&tmp, obj, sizeof(t_object));
    auto x = new(obj) T{std::forward<Args>(args)...};
    memcpy(x, &tmp, sizeof(t_object));

    return x;
  }
  return nullptr;
}

t_jit_realsense *jit_realsense_new(void)
{
    auto obj = jit_new<t_jit_realsense>(s_jit_realsense_class);
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

    // Compare this matrix with the one in out_minfo
    if(!compare_matrix_info(out_minfo, expected))
    {
        // Change the matrix if it is different
        jit_object_method(out_matrix, _jit_sym_setinfo, &expected);
        // c.f. _jit_sym_setinfo usage in jit.openni
        jit_object_method(out_matrix, _jit_sym_setinfo, &expected);
    }

    // Return a pointer to the data
    char* out_bp{};
    jit_object_method(out_matrix, _jit_sym_getdata, &out_bp);
    return out_bp;
}


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
            auto image = (const uint16_t *) rs_matrix.get_data();
            auto matrix_out = (long*) max_matrix;

            std::copy(image, image + size, matrix_out);
        }
};

// 8 bit case
template<>
struct copier<RS2_FORMAT_Y8>
{
        void operator()(int size, const rs2::frame& rs_matrix, char* max_matrix)
        {
            auto image = (const uint8_t *) rs_matrix.get_data();
            auto matrix_out = (char*) max_matrix;

            std::copy(image, image + size, matrix_out);
        }
};

// Float case
/*
template<>
struct copier<RS2_FORMAT_XYZ32F>
{
        void operator()(int size, const rs2::frame& rs_matrix, char* max_matrix)
        {
            auto image = (const rs::float3 *) rs_matrix.get_data();
            auto matrix_out = (float*) max_matrix;

            int j = 0;
            for(int i = 0; i < size / 3; i++)
            {
                matrix_out[j++] = image[i].x;
                matrix_out[j++] = image[i].y;
                matrix_out[j++] = image[i].z;
            }
        }
};
*/
/*
void do_copy(rs::format fmt, int size, const void* rs_matrix, char* max_matrix)
{
    switch(fmt)
    {
        case RS2_FORMAT_z16:
        case RS2_FORMAT_y16:
        case RS2_FORMAT_disparity16:
             return copier<RS2_FORMAT_z16>{}(size, rs_matrix, max_matrix);
        case RS2_FORMAT_y8:
        case RS2_FORMAT_rgb8:
        case RS2_FORMAT_bgr8:
        case RS2_FORMAT_rgba8:
        case RS2_FORMAT_bgra8:
        case RS2_FORMAT_yuyv:
            return copier<RS2_FORMAT_y8>{}(size, rs_matrix, max_matrix);
        case RS2_FORMAT_xyz32f:
            return copier<RS2_FORMAT_xyz32f>{}(size, rs_matrix, max_matrix);
        // Weird cases :
        case RS2_FORMAT_raw10:
        case RS2_FORMAT_any:
            throw std::logic_error{"raw10, any unhandled"};
    }
}
*/

void do_copy(rs2_stream str, int size, const rs2::frame& rs_matrix, char* max_matrix)
{
    switch(str)
    {
        case RS2_STREAM_DEPTH: return copier<RS2_FORMAT_Z16>{}(size, rs_matrix, max_matrix);
        case RS2_STREAM_INFRARED:
        case RS2_STREAM_COLOR: return copier<RS2_FORMAT_Y8>{}(size, rs_matrix, max_matrix);
    }
}
void compute_output(t_jit_realsense *x, void *matrix, const jit_rs_streaminfo& info, const rs2::frameset& frames)
{
    const auto num_planes = num_planes_from_stream(info.stream);
    const auto sym = symbol_from_stream(info.stream);
    const rs2_stream stream = info.stream;

    auto lock = jit_object_method(matrix, _jit_sym_lock, 1);

    t_jit_matrix_info out_minfo;
    jit_object_method(matrix, _jit_sym_getinfo, &out_minfo);

    // Get the realsense informations and compare them
    // with the current matrix.
    const auto stream_profile = x->profile.get_stream(stream).as<rs2::video_stream_profile>();
    char* out_bp = make_n_plane_matrix(out_minfo, matrix, stream_profile,
                                       num_planes,
                                       sym);

    // Copy the data in the Max Matrix
    int size = stream_profile.height() * stream_profile.width() * num_planes;
    do_copy(info.stream, size, frames.first(stream), out_bp);

    jit_object_method(matrix, _jit_sym_lock, lock);
}

t_jit_err jit_realsense_matrix_calc(t_jit_realsense* x, void *, void *outputs)
try
{
    // Get and check the data.
    if(!x || !x->dev)
    {
        error("No device");
        return JIT_ERR_INVALID_PTR;
    }

    if(x->device != x->device_cache)
    {
        x->rebuild();
    }
    else if(x->out_count != x->out_count_cache)
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

    for(int i = 0; i < x->out_count; i++)
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
    return JIT_ERR_GENERIC;
}
