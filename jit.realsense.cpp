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
    double		gain{};	// our attribute (multiplied against each cell in the matrix)

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
    long			attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    t_jit_object	*attr;
    t_jit_object	*mop;

    s_jit_realsense_class = jit_class_new("jit_realsense", (method)jit_realsense_new, (method)jit_realsense_free, sizeof(t_jit_realsense), 0);

    // add matrix operator (mop)
    mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 1, 1); // args are  num inputs and num outputs
    jit_class_addadornment(s_jit_realsense_class, mop);

    // add method(s)
    jit_class_addmethod(s_jit_realsense_class, (method)jit_realsense_matrix_calc, "matrix_calc", A_CANT, 0);

    // add attribute(s)
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset,
                                          "gain",
                                          _jit_sym_float64,
                                          attrflags,
                                          (method)NULL, (method)NULL,
                                          calcoffset(t_jit_realsense, gain));
    jit_class_addattr(s_jit_realsense_class, attr);

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
        x->gain = 0.0;
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

t_jit_err jit_realsense_matrix_calc(t_jit_realsense *x, void *inputs, void *outputs)
{
    t_jit_err			err = JIT_ERR_NONE;
    long				in_savelock;
    long				out_savelock;
    t_jit_matrix_info	in_minfo;
    t_jit_matrix_info	out_minfo;
    char				*in_bp;
    char				*out_bp;
    long				i;
    long				dimcount;
    long				planecount;
    long				dim[JIT_MATRIX_MAX_DIMCOUNT];
    void				*in_matrix;
    void				*out_matrix;

    in_matrix 	= jit_object_method(inputs,_jit_sym_getindex,0);
    out_matrix 	= jit_object_method(outputs,_jit_sym_getindex,0);

    if (x && in_matrix && out_matrix) {
        in_savelock = (long) jit_object_method(in_matrix, _jit_sym_lock, 1);
        out_savelock = (long) jit_object_method(out_matrix, _jit_sym_lock, 1);

        jit_object_method(in_matrix, _jit_sym_getinfo, &in_minfo);
        jit_object_method(out_matrix, _jit_sym_getinfo, &out_minfo);

        jit_object_method(in_matrix, _jit_sym_getdata, &in_bp);
        jit_object_method(out_matrix, _jit_sym_getdata, &out_bp);

        if (!in_bp) {
            err=JIT_ERR_INVALID_INPUT;
            goto out;
        }
        if (!out_bp) {
            err=JIT_ERR_INVALID_OUTPUT;
            goto out;
        }
        if (in_minfo.type != out_minfo.type) {
            err = JIT_ERR_MISMATCH_TYPE;
            goto out;
        }

        //get dimensions/planecount
        dimcount   = out_minfo.dimcount;
        planecount = out_minfo.planecount;

        for (i=0; i<dimcount; i++) {
            //if dimsize is 1, treat as infinite domain across that dimension.
            //otherwise truncate if less than the output dimsize
            dim[i] = out_minfo.dim[i];
            if ((in_minfo.dim[i]<dim[i]) && in_minfo.dim[i]>1) {
                dim[i] = in_minfo.dim[i];
            }
        }


        // Get the realsense data
        if(!x->dev)
        {
            post("no data");
            return JIT_ERR_DATA_UNAVAILABLE;
        }
        const uint16_t * depth_image = (const uint16_t *)x->dev->get_frame_data(rs::stream::depth);
        const rs::intrinsics depth_intrin = x->dev->get_stream_intrinsics(rs::stream::depth);

        out_minfo.planecount = 1;
        out_minfo.dimcount = 2;
        out_minfo.dim[0] = depth_intrin.height;
        out_minfo.dim[1] = depth_intrin.width;
        out_minfo.type = _jit_sym_float32;

        jit_object_method(out_matrix, _jit_sym_setinfo, &out_minfo);
        jit_object_method(out_matrix, _jit_sym_setinfo, &out_minfo);
        jit_object_method(out_matrix, _jit_sym_getdata, &out_bp);

        auto float_ptr_out = (float*) out_bp;

        int i = 0;

        x->dev->wait_for_frames();

        // Copy the data as float32 in the Max Matrix
        for(int dy = 0; dy < depth_intrin.height; ++dy)
        {
            for(int dx = 0; dx < depth_intrin.width; ++dx)
            {
                uint16_t depth_value = depth_image[dy * depth_intrin.width + dx];
                //if(depth_value == 0) continue;
                float_ptr_out[i] = float(depth_value) ;
                //post("%f\n", float_ptr_out[i]);
                i++;
            }
        }

//        jit_parallel_ndim_simplecalc2((method)jit_realsense_calculate_ndim,
//                                      x, dimcount, dim, planecount, &in_minfo, in_bp, &out_minfo, out_bp,
//                                      0 /* flags1 */, 0 /* flags2 */);


    }
    else
        return JIT_ERR_INVALID_PTR;

out:
    jit_object_method(out_matrix,_jit_sym_lock,out_savelock);
    jit_object_method(in_matrix,_jit_sym_lock,in_savelock);
    return err;
}


// We are using a C++ template to process a vector of the matrix for any of the given types.
// Thus, we don't need to duplicate the code for each datatype.
template<typename T>
void jit_realsense_vector(t_jit_realsense *x, long n, t_jit_op_info *in, t_jit_op_info *out)
{
    double	gain = x->gain;
    T		*ip;
    T		*op;
    long	is,
            os;
    T		tmp;

    ip = ((T *)in->p);
    op = ((T *)out->p);
    is = in->stride;
    os = out->stride;

    if ((is==1) && (os==1)) {
        ++n;
        --op;
        --ip;
        while (--n) {
            tmp = *++ip;
            *++op = tmp * gain;
        }
    }
    else {
        while (n--) {
            tmp = *ip;
            *op = tmp * gain;
            ip += is;
            op += os;
        }
    }
}


// We also use a C+ template for the loop that wraps the call to jit_realsense_vector(),
// further reducing code duplication in jit_realsense_calculate_ndim().
// The calls into these templates should be inlined by the compiler, eliminating concern about any added function call overhead.
template<typename T>
void jit_realsense_loop(t_jit_realsense *x, long n, t_jit_op_info *in_opinfo, t_jit_op_info *out_opinfo, t_jit_matrix_info *in_minfo, t_jit_matrix_info *out_minfo, char *bip, char *bop, long *dim, long planecount, long datasize)
{
    long	i;
    long	j;

    for (i=0; i<dim[1]; i++) {
        for (j=0; j<planecount; j++) {
            in_opinfo->p  = bip + i * in_minfo->dimstride[1]  + (j % in_minfo->planecount) * datasize;
            out_opinfo->p = bop + i * out_minfo->dimstride[1] + (j % out_minfo->planecount) * datasize;
            jit_realsense_vector<T>(x, n, in_opinfo, out_opinfo);
        }
    }
}


void jit_realsense_calculate_ndim(t_jit_realsense *x, long dimcount, long *dim, long planecount, t_jit_matrix_info *in_minfo, char *bip, t_jit_matrix_info *out_minfo, char *bop)
{
    return;
    long			i;
    long			n;
    char			*ip;
    char			*op;
    t_jit_op_info	in_opinfo;
    t_jit_op_info	out_opinfo;

    if (dimcount < 1)
        return; // safety

    switch (dimcount) {
    case 1:
        dim[1]=1;
        // (fall-through to next case is intentional)
    case 2:
        // if planecount is the same then flatten planes - treat as single plane data for speed
        n = dim[0];
        if ((in_minfo->dim[0] > 1) && (out_minfo->dim[0] > 1) && (in_minfo->planecount == out_minfo->planecount)) {
            in_opinfo.stride = 1;
            out_opinfo.stride = 1;
            n *= planecount;
            planecount = 1;
        }
        else {
            in_opinfo.stride =  in_minfo->dim[0]>1  ? in_minfo->planecount  : 0;
            out_opinfo.stride = out_minfo->dim[0]>1 ? out_minfo->planecount : 0;
        }

        if (in_minfo->type == _jit_sym_char)
            jit_realsense_loop<uchar>(x, n, &in_opinfo, &out_opinfo, in_minfo, out_minfo, bip, bop, dim, planecount, 1);
        else if (in_minfo->type == _jit_sym_long)
            jit_realsense_loop<int>(x, n, &in_opinfo, &out_opinfo, in_minfo, out_minfo, bip, bop, dim, planecount, 4);
        else if (in_minfo->type == _jit_sym_float32)
            jit_realsense_loop<float>(x, n, &in_opinfo, &out_opinfo, in_minfo, out_minfo, bip, bop, dim, planecount, 4);
        else if (in_minfo->type == _jit_sym_float64)
            jit_realsense_loop<double>(x, n, &in_opinfo, &out_opinfo, in_minfo, out_minfo, bip, bop, dim, planecount, 8);
        break;
    default:
        for	(i=0; i<dim[dimcount-1]; i++) {
            ip = bip + i * in_minfo->dimstride[dimcount-1];
            op = bop + i * out_minfo->dimstride[dimcount-1];
            jit_realsense_calculate_ndim(x, dimcount-1, dim, planecount, in_minfo, ip, out_minfo, op);
        }
    }
}
