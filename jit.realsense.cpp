#include <librealsense/rs.hpp>
#include <vector>
namespace rs
{
using exception = error;
}
#include "jit.common.h"
// Our Jitter object instance data
typedef struct _jit_realsense {
        t_object	ob{};
        rs::device* dev{};

        static rs::context& context()
        {
            static rs::context ctx;
            return ctx;
        }
} t_jit_realsense;


// prototypes
BEGIN_USING_C_LINKAGE
t_jit_err		jit_realsense_init				(void);
t_jit_realsense	*jit_realsense_new				(void);
void			jit_realsense_free				(t_jit_realsense *x);
t_jit_err		jit_realsense_matrix_calc		(t_jit_realsense *x, void *inputs, void *outputs);
void			jit_realsense_calculate_ndim	(t_jit_realsense *x, long dim, long *dimsize, long planecount, t_jit_matrix_info *in_minfo, char *bip, t_jit_matrix_info *out_minfo, char *bop);
END_USING_C_LINKAGE


// globals
static void *s_jit_realsense_class = NULL;

/************************************************************************************/

t_jit_err jit_realsense_init(void)
{
    t_jit_object	*mop;

    s_jit_realsense_class = jit_class_new("jit_realsense", (method)jit_realsense_new, (method)jit_realsense_free, sizeof(t_jit_realsense), 0);

    // add matrix operator (mop)
    mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 0, 3); // args are  num inputs and num outputs
    jit_class_addadornment(s_jit_realsense_class, mop);

    // add method(s)
    jit_class_addmethod(s_jit_realsense_class, (method)jit_realsense_matrix_calc, "matrix_calc", A_CANT, 0);

    // finalize class
    jit_class_register(s_jit_realsense_class);
    return JIT_ERR_NONE;
}


/************************************************************************************/
// Object Life Cycle

t_jit_realsense *jit_realsense_new(void)
{
    auto& ctx = _jit_realsense::context();
    auto n_dev = ctx.get_device_count();
    t_jit_realsense	*x = NULL;
    post("There are %d connected RealSense devices.\n", n_dev);

    x = (t_jit_realsense *)jit_object_alloc(s_jit_realsense_class);
    if (x) {
        if(n_dev > 0)
        {
            try {
                // TODO add used device as a parameter
                x->dev = ctx.get_device(0);

                post("\nUsing device 0, an %s\n", x->dev->get_name());
                post("    Serial number: %s\n", x->dev->get_serial());
                post("    Firmware version: %s\n", x->dev->get_firmware_version());

                if(!x->dev->is_streaming())
                {
                    x->dev->enable_stream(rs::stream::color, rs::preset::best_quality);
                    x->dev->enable_stream(rs::stream::depth, rs::preset::best_quality);
                    x->dev->enable_stream(rs::stream::infrared, rs::preset::best_quality);
                    x->dev->start();
                }
            }
            catch(const rs::exception & e)
            {
                post("%s\n", e.what());
                if(x->dev)
                    x->dev = nullptr;
            }
        }
    }
    return x;
}


void jit_realsense_free(t_jit_realsense *x)
{
    if(x->dev && x->dev->is_streaming())
    {
        x->dev->stop();
        x->dev->disable_stream(rs::stream::depth);
    }
}


/************************************************************************************/
// Methods bound to input/inlets

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
        const rs::intrinsics& stream,
        int n_plane,
        t_symbol* type)
{
    // First fill a matrix_info with the values we want
    t_jit_matrix_info expected = out_minfo;

    expected.planecount = n_plane;
    expected.dimcount = 2;
    expected.dim[0] = stream.width;
    expected.dim[1] = stream.height;
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

// This is parametrized on a type that will give all the relevant
// information needed to convert from the RS's data to Max's matrix format
template<typename T>
void compute_output(t_jit_realsense *x, void *matrix)
{
    long lock = (long) jit_object_method(matrix, _jit_sym_lock, 1);

    t_jit_matrix_info out_minfo;
    jit_object_method(matrix, _jit_sym_getinfo, &out_minfo);

    // Get the realsense informations and compare them
    // with the current matrix.
    const rs::intrinsics depth_intrin = x->dev->get_stream_intrinsics(T::stream_type);
    char* out_bp = make_n_plane_matrix(out_minfo, matrix, depth_intrin, T::num_planes, T::symbol());

    // Do the actual copy
    auto depth_image = (const typename T::realsense_type *) x->dev->get_frame_data(T::stream_type);
    auto matrix_out = (typename T::max_type*) out_bp;

    // Copy the data as long in the Max Matrix
    int size = depth_intrin.height * depth_intrin.width * T::num_planes;
    std::copy(depth_image, depth_image + size, matrix_out);

    jit_object_method(matrix, _jit_sym_lock, lock);
}

struct DepthInfo
{
        using max_type = long;
        using realsense_type = uint16_t;
        static auto symbol() { return _jit_sym_long; }
        static const constexpr int num_planes = 1;
        static const constexpr rs::stream stream_type = rs::stream::depth;
};

struct ColorInfo
{
        using max_type = char;
        using realsense_type = uint8_t;
        static auto symbol() { return _jit_sym_char; }
        static const constexpr int num_planes = 3;
        static const constexpr rs::stream stream_type = rs::stream::color;
};

struct IR1Info
{
        using max_type = char;
        using realsense_type = uint8_t;
        static auto symbol() { return _jit_sym_char; }
        static const constexpr int num_planes = 1;
        static const constexpr rs::stream stream_type = rs::stream::infrared;
};


t_jit_err jit_realsense_matrix_calc(t_jit_realsense *x, void *inputs, void *outputs)
{
    // Get and check the data.
    if(!x || !x->dev)
    {
        error("No device");
        return JIT_ERR_INVALID_PTR;
    }

    // Fetch new frame from the realsense
    x->dev->wait_for_frames();

    { // Depth -> first outlet
        auto matrix = jit_object_method(outputs, _jit_sym_getindex, 0);
        if (!matrix)
        {
            error("No depth");
            return JIT_ERR_INVALID_PTR;
        }
        compute_output<DepthInfo>(x, matrix);
    }

    { // Color -> second outlet
        auto matrix = jit_object_method(outputs, _jit_sym_getindex, 1);
        if (!matrix)
        {
            error("No color");
            return JIT_ERR_INVALID_PTR;
        }
        compute_output<ColorInfo>(x, matrix);
    }

    { // IR1 -> third outlet
        auto matrix = jit_object_method(outputs, _jit_sym_getindex, 2);
        if (!matrix)
        {
            error("No color");
            return JIT_ERR_INVALID_PTR;
        }
        compute_output<IR1Info>(x, matrix);
    }

    return JIT_ERR_NONE;
}
