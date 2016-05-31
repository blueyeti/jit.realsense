#include <librealsense/rs.hpp>
#include <vector>
#include <array>
#include <cstdint>
#include "jit.common.h"

// To be able to iterate over rs::stream

static const constexpr int jit_realsense_num_outlets = 6;

std::pair<rs::stream, rs::stream> native_streams(rs::stream other)
{
    using namespace std;
    switch(other)
    {
        case rs::stream::depth: return make_pair(rs::stream::depth, rs::stream::depth);
        case rs::stream::color: return make_pair(rs::stream::color, rs::stream::color);
        case rs::stream::infrared: return make_pair(rs::stream::infrared, rs::stream::infrared);
        case rs::stream::infrared2: return make_pair(rs::stream::infrared2, rs::stream::infrared2);
        case rs::stream::points: return make_pair(rs::stream::depth, rs::stream::depth);
        case rs::stream::rectified_color: return make_pair(rs::stream::color, rs::stream::depth);
        case rs::stream::color_aligned_to_depth: return make_pair(rs::stream::color, rs::stream::depth);
        case rs::stream::infrared2_aligned_to_depth: return make_pair(rs::stream::infrared2, rs::stream::depth);
        case rs::stream::depth_aligned_to_color: return make_pair(rs::stream::depth, rs::stream::color);
        case rs::stream::depth_aligned_to_rectified_color: return make_pair(rs::stream::depth, rs::stream::color);
        case rs::stream::depth_aligned_to_infrared2: return make_pair(rs::stream::depth, rs::stream::infrared2);
    }
}


rs::format best_format(rs::stream other)
{
    using namespace std;
    switch(other)
    {
        case rs::stream::depth_aligned_to_color:
        case rs::stream::depth_aligned_to_rectified_color:
        case rs::stream::depth_aligned_to_infrared2:
        case rs::stream::depth: return rs::format::z16;

        case rs::stream::infrared:
        case rs::stream::infrared2_aligned_to_depth:
        case rs::stream::infrared2: return rs::format::y8;

        case rs::stream::points: return rs::format::xyz32f;

        case rs::stream::rectified_color:
        case rs::stream::color_aligned_to_depth:
        case rs::stream::color: return rs::format::rgb8;
    }
}


struct jit_rs_streaminfo
{
        long stream = (long)rs::stream::depth;
        long format = (long)rs::format::z16;
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
        t_object	ob{};
        rs::device* dev{};

        long device = 0;
        long out_count = 1;

        std::array<jit_rs_streaminfo, jit_realsense_num_outlets> outputs;

        long device_cache = 0;
        long out_count_cache = 1;
        std::array<jit_rs_streaminfo, jit_realsense_num_outlets> outputs_cache;

        void construct()
        {
            device = 0;
            out_count = 1;
            outputs = std::array<jit_rs_streaminfo, jit_realsense_num_outlets>{};

            device_cache = 0;
            out_count = 1;
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
            auto n_dev = ctx.get_device_count();
            post("There are %d connected RealSense devices.\n", n_dev);

            if(n_dev <= device)
            {
                post("Device %d is not connected.", device);
                return;
            }

            dev = ctx.get_device((int)device);

            post("\nUsing device %d, an %s\n", device, dev->get_name());
            post("    Serial number: %s\n", dev->get_serial());
            post("    Firmware version: %s\n", dev->get_firmware_version());

            rebuild_streams();

            device_cache = device;
            out_count_cache = out_count;
        }
        catch(const std::exception & e)
        {
            error("%s\n", e.what());
            if(dev)
                dev = nullptr;
        }

        void rebuild_streams()
        try
        {
            cleanup();

            // First enable all native streams
            for(int i = 0; i < out_count; i++)
            {
                jit_rs_streaminfo& out = outputs[(std::size_t)i];
                auto streams = native_streams((rs::stream)out.stream);
                if(streams.first == streams.second && streams.first == (rs::stream) out.stream) // Native stream
                {
                    auto format = best_format((rs::stream)streams.first);
                    dev->enable_stream((rs::stream) out.stream, (int)out.dimensions[0], (int)out.dimensions[1], format, (int)out.rate);
                }
            }

            // Then enable aligned streams, since they require native streams.
            // If a native stream is missing, it is created with decent defaults.
            for(int i = 0; i < out_count; i++)
            {
                jit_rs_streaminfo& out = outputs[(std::size_t)i];
                auto streams = native_streams((rs::stream)out.stream);
                if(streams.first != streams.second ||  streams.first != (rs::stream) out.stream) // Aligned stream
                {
                    if(!dev->is_stream_enabled(streams.first))
                        dev->enable_stream(streams.first, rs::preset::best_quality);
                    if(!dev->is_stream_enabled(streams.second))
                        dev->enable_stream(streams.second, rs::preset::best_quality);
                }
            }

            outputs_cache = outputs;
            if(out_count > 0)
                dev->start();
        }
        catch(const std::exception & e)
        {
            error("%s\n", e.what());
        }

        static rs::context& context()
        {
            static rs::context ctx;
            return ctx;
        }

        void cleanup()
        {
            if(!dev)
                return;

            if(dev->is_streaming())
                dev->stop();

            for(int i = 0; i < 4; i++)
                dev->disable_stream((rs::stream)i);

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
    atom_setsym(aaa,gensym_tr(arg));
    class_attr_enumindex_rec(aaa + 1, std::forward<Args>(args)...);
}

template<typename... Args>
void class_attr_enumindex(t_class* theclass, std::string attrname, Args&&... args)
{
    constexpr int num = sizeof...(Args);
    t_atom aaa[num];
    CLASS_ATTR_STYLE(theclass, attrname.c_str(), 0, "enumindex");
    class_attr_enumindex_rec(aaa, std::forward<Args>(args)...);
    CLASS_ATTR_ATTR_ATOMS(theclass, attrname.c_str(), "enumvals", USESYM(atom), 0, num, aaa);
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
                         "Depth", "Color", "Infrared", "Infrared 2",
                         "Points", "Rectified color", "Color -> Depth",
                         "Infrared 2 -> Depth", "Depth -> Color", "Depth -> Rectified color",
                         "Depth -> Infrared 2");
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
                                 "Depth", "Color", "Infrared", "Infrared 2",
                                 "Points", "Rectified color", "Color -> Depth",
                                 "Infrared 2 -> Depth", "Depth -> Color", "Depth -> Rectified color",
                                 "Depth -> Infrared 2");
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

t_jit_realsense *jit_realsense_new(void)
{
    t_jit_realsense	*x = NULL;

    x = (t_jit_realsense *)jit_object_alloc(s_jit_realsense_class);
    if (x)
    {
        x->construct();
        x->rebuild();
    }
    return x;
}

void jit_realsense_free(t_jit_realsense *x)
{
    if(x->dev && x->dev->is_streaming())
    {
        x->dev->stop();
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
        const rs::intrinsics& intrin,
        int n_plane,
        t_symbol* type)
{
    // First fill a matrix_info with the values we want
    t_jit_matrix_info expected = out_minfo;

    expected.planecount = n_plane;
    expected.dimcount = 2;
    expected.dim[0] = intrin.width;
    expected.dim[1] = intrin.height;
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


int num_planes_from_stream(rs::stream other)
{
    using namespace std;
    switch(other)
    {
        case rs::stream::depth: return 1;
        case rs::stream::color: return 3;
        case rs::stream::infrared: return 1;
        case rs::stream::infrared2: return 1;
        case rs::stream::points: return 3;
        case rs::stream::rectified_color: return 3;
        case rs::stream::color_aligned_to_depth: return 3;
        case rs::stream::infrared2_aligned_to_depth: return 1;
        case rs::stream::depth_aligned_to_color: return 1;
        case rs::stream::depth_aligned_to_rectified_color: return 1;
        case rs::stream::depth_aligned_to_infrared2: return 1;
    }
}

int num_planes_from_format(long format)
{
    switch((rs::format) format)
    {
        case rs::format::any:
            throw std::logic_error{"any unhandled"};
        case rs::format::z16:
        case rs::format::disparity16:
        case rs::format::y8:
        case rs::format::y16:
        case rs::format::yuyv:
            return 1;
        case rs::format::rgb8:
        case rs::format::bgr8:
        case rs::format::xyz32f:
            return 3;
        case rs::format::rgba8:
        case rs::format::bgra8:
        case rs::format::raw10:
            return 4;
    }
}

t_symbol * symbol_from_format(long format)
{
    switch((rs::format) format)
    {
        case rs::format::z16:
        case rs::format::y16:
        case rs::format::disparity16:
            return _jit_sym_long;
        case rs::format::y8:
        case rs::format::rgb8:
        case rs::format::bgr8:
        case rs::format::rgba8:
        case rs::format::bgra8:
        case rs::format::yuyv:
            return _jit_sym_char;
        case rs::format::xyz32f:
            return _jit_sym_float32;
        // Weird cases :
        case rs::format::raw10:
        case rs::format::any:
            throw std::logic_error{"raw10, any unhandled"};
    }
}

t_symbol * symbol_from_stream(rs::stream stream)
{
    switch(stream)
    {
        case rs::stream::depth_aligned_to_color:
        case rs::stream::depth_aligned_to_rectified_color:
        case rs::stream::depth_aligned_to_infrared2:
        case rs::stream::depth:
            return _jit_sym_long;

        case rs::stream::infrared:
        case rs::stream::infrared2:
        case rs::stream::rectified_color:
        case rs::stream::color_aligned_to_depth:
        case rs::stream::infrared2_aligned_to_depth:
        case rs::stream::color:
            return _jit_sym_char;

        case rs::stream::points:
            return _jit_sym_float32;
    }
}

template<rs::format>
struct copier;

// 16 bit case
template<>
struct copier<rs::format::z16>
{
        void operator()(int size, const void* rs_matrix, char* max_matrix)
        {
            auto image = (const uint16_t *) rs_matrix;
            auto matrix_out = (long*) max_matrix;

            std::copy(image, image + size, matrix_out);
        }
};

// 8 bit case
template<>
struct copier<rs::format::y8>
{
        void operator()(int size, const void* rs_matrix, char* max_matrix)
        {
            auto image = (const uint8_t *) rs_matrix;
            auto matrix_out = (char*) max_matrix;

            std::copy(image, image + size, matrix_out);
        }
};

// Float case
template<>
struct copier<rs::format::xyz32f>
{
        void operator()(int size, const void* rs_matrix, char* max_matrix)
        {
            auto image = (const rs::float3 *) rs_matrix;
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
/*
void do_copy(rs::format fmt, int size, const void* rs_matrix, char* max_matrix)
{
    switch(fmt)
    {
        case rs::format::z16:
        case rs::format::y16:
        case rs::format::disparity16:
             return copier<rs::format::z16>{}(size, rs_matrix, max_matrix);
        case rs::format::y8:
        case rs::format::rgb8:
        case rs::format::bgr8:
        case rs::format::rgba8:
        case rs::format::bgra8:
        case rs::format::yuyv:
            return copier<rs::format::y8>{}(size, rs_matrix, max_matrix);
        case rs::format::xyz32f:
            return copier<rs::format::xyz32f>{}(size, rs_matrix, max_matrix);
        // Weird cases :
        case rs::format::raw10:
        case rs::format::any:
            throw std::logic_error{"raw10, any unhandled"};
    }
}
*/

void do_copy(rs::stream str, int size, const void* rs_matrix, char* max_matrix)
{
    switch(str)
    {
        case rs::stream::depth_aligned_to_color:
        case rs::stream::depth_aligned_to_rectified_color:
        case rs::stream::depth_aligned_to_infrared2:
        case rs::stream::depth: return copier<rs::format::z16>{}(size, rs_matrix, max_matrix);

        case rs::stream::infrared:
        case rs::stream::infrared2:
        case rs::stream::rectified_color:
        case rs::stream::color_aligned_to_depth:
        case rs::stream::infrared2_aligned_to_depth:
        case rs::stream::color: return copier<rs::format::y8>{}(size, rs_matrix, max_matrix);

        case rs::stream::points: return copier<rs::format::xyz32f>{}(size, rs_matrix, max_matrix);
    }
}
void compute_output(t_jit_realsense *x, void *matrix, const jit_rs_streaminfo& info)
{
    const auto num_planes = num_planes_from_stream((rs::stream)info.stream);
    const auto sym = symbol_from_stream((rs::stream)info.stream);
    const rs::stream stream = (rs::stream)info.stream;

    long lock = (long) jit_object_method(matrix, _jit_sym_lock, 1);

    t_jit_matrix_info out_minfo;
    jit_object_method(matrix, _jit_sym_getinfo, &out_minfo);

    // Get the realsense informations and compare them
    // with the current matrix.
    const rs::intrinsics intrin = x->dev->get_stream_intrinsics(stream);
    char* out_bp = make_n_plane_matrix(out_minfo, matrix, intrin,
                                       num_planes,
                                       sym);

    // Copy the data in the Max Matrix
    int size = intrin.height * intrin.width * num_planes;
    do_copy((rs::stream) info.stream, size, x->dev->get_frame_data(stream), out_bp);

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
    x->dev->wait_for_frames();

    for(int i = 0; i < x->out_count; i++)
    {
        if (auto matrix = jit_object_method(outputs, _jit_sym_getindex, i))
        {
            compute_output(x, matrix, x->outputs[i]);
        }
    }

    return JIT_ERR_NONE;
}
catch(const std::exception & e)
{
    error("%s\n", e.what());
    return JIT_ERR_GENERIC;
}
