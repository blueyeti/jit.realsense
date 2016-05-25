#include "jit.common.h"
#include "max.jit.mop.h"


// Max object instance data
// Note: most instance data is in the Jitter object which we will wrap
typedef struct _max_jit_realsense {
    t_object	ob;
    void		*obex;
} t_max_jit_realsense;


// prototypes
BEGIN_USING_C_LINKAGE
t_jit_err	jit_realsense_init(void);
void		*max_jit_realsense_new(t_symbol *s, long argc, t_atom *argv);
void		max_jit_realsense_free(t_max_jit_realsense *x);
END_USING_C_LINKAGE

// globals
static void	*max_jit_realsense_class = NULL;


/************************************************************************************/

void ext_main(void *r)
{
    t_class *max_class, *jit_class;

    jit_realsense_init();

    max_class = class_new("jit.realsense", (method)max_jit_realsense_new, (method)max_jit_realsense_free, sizeof(t_max_jit_realsense), NULL, A_GIMME, 0);
    max_jit_class_obex_setup(max_class, calcoffset(t_max_jit_realsense, obex));

    jit_class = (maxclass*)jit_class_findbyname(gensym("jit_realsense"));
    max_jit_class_mop_wrap(max_class, jit_class, 0);			// attrs & methods for name, type, dim, planecount, bang, outputmatrix, etc
    max_jit_class_wrap_standard(max_class, jit_class, 0);		// attrs & methods for getattributes, dumpout, maxjitclassaddmethods, etc

    class_addmethod(max_class, (method)max_jit_mop_assist, "assist", A_CANT, 0);	// standard matrix-operator (mop) assist fn

    class_register(CLASS_BOX, max_class);
    max_jit_realsense_class = max_class;
}


/************************************************************************************/
// Object Life Cycle

void *max_jit_realsense_new(t_symbol *s, long argc, t_atom *argv)
{
    t_max_jit_realsense	*x;
    void			*o;

    x = (t_max_jit_realsense *)max_jit_object_alloc((maxclass*)max_jit_realsense_class, gensym("jit_realsense"));
    if (x) {
        o = jit_object_new(gensym("jit_realsense"));
        if (o) {
            max_jit_mop_setup_simple(x, o, argc, argv);
            max_jit_attr_args(x, argc, argv);
        }
        else {
            jit_object_error((t_object *)x, "jit.realsense: could not allocate object");
            object_free((t_object *)x);
            x = NULL;
        }
    }
    return (x);
}


void max_jit_realsense_free(t_max_jit_realsense *x)
{
    max_jit_mop_free(x);
    jit_object_free(max_jit_obex_jitob_get(x));
    max_jit_object_free(x);
}

