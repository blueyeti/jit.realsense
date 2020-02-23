#include "jit.realsense.hpp"
#include <jit.common.h>
#include <max.jit.mop.h>

struct t_max_jit_realsense
{
    //static inline void* max_class = nullptr;
    static void* max_class;
    t_object ob;
    void *obex;
};

void* t_max_jit_realsense::max_class = nullptr;

// Taken from jit.noise example
void max_jit_realsense_outputmatrix(t_max_jit_realsense *x)
{
  long outputmode = max_jit_mop_getoutputmode(x);
  void *mop = max_jit_obex_adornment_get(x,_jit_sym_jit_mop);

  if(!outputmode || !mop)
    return;

  if(outputmode != 1)
  {
    max_jit_mop_outputmatrix(x);
    return;
  }

  auto err = (t_jit_err)jit_object_method(
        max_jit_obex_jitob_get(x),
        _jit_sym_matrix_calc,
        jit_object_method(mop,_jit_sym_getinputlist),
        jit_object_method(mop,_jit_sym_getoutputlist));

  if (err)
  {
    jit_error_code(x,err);
  }
  else
  {
    max_jit_mop_outputmatrix(x);
  }
}

void *max_jit_realsense_new(t_symbol *, long argc, t_atom *argv)
{
  auto x = max_jit_object_alloc((maxclass*)t_max_jit_realsense::max_class, gensym("jit_realsense"));
  if (x)
  {
    auto o = jit_object_new(gensym("jit_realsense"));
    if (o)
    {
      max_jit_mop_setup_simple(x, o, argc, argv);
      max_jit_attr_args(x, (short)argc, argv);
    }
    else
    {
      jit_object_error((t_object *)x, (char*)"jit.realsense: could not allocate object");
      object_free((t_object *)x);
      x = nullptr;
    }
  }
  return x;
}

void max_jit_realsense_free(t_max_jit_realsense *x)
{
  max_jit_mop_free(x);
  jit_object_free(max_jit_obex_jitob_get(x));
  max_jit_object_free(x);
}

void ext_main(void *)
{
  jit_realsense_init();

  auto max_class = class_new("jit.realsense", (method)max_jit_realsense_new, (method)max_jit_realsense_free, sizeof(t_max_jit_realsense), NULL, A_GIMME, 0);
  max_jit_class_obex_setup(max_class, calcoffset(t_max_jit_realsense, obex));

  auto jit_class = (maxclass*)jit_class_findbyname(gensym("jit_realsense"));
  max_jit_class_mop_wrap(max_class, jit_class, MAX_JIT_MOP_FLAGS_OWN_OUTPUTMATRIX|MAX_JIT_MOP_FLAGS_OWN_JIT_MATRIX);
  max_jit_class_wrap_standard(max_class, jit_class, 0);

  max_jit_class_addmethod_usurp_low(max_class, (method) max_jit_realsense_outputmatrix, (char*)"outputmatrix");
  class_addmethod(max_class, (method)max_jit_mop_assist, "assist", A_CANT, 0);
  class_register(CLASS_BOX, max_class);
  t_max_jit_realsense::max_class = max_class;
}
