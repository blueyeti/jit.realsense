/*
	Copyright 2001 - Cycling '74
	Joshua Kit Clayton jkc@cycling74.com
*/

#include "jit.common.h"
#include <librealsense/rs.h>

extern "C"
{
typedef struct _max_jit_hello
{
	t_object 		ob;
	void			*obex;
} t_max_jit_hello;

t_messlist *class_max_jit_hello;

void max_jit_hello_assist(t_max_jit_hello *x, void *b, long m, long a, char *s);
void *max_jit_hello_new(t_symbol *s, long argc, t_atom *argv);
void max_jit_hello_free(t_max_jit_hello *x);

//from jit.hello.c
t_jit_err jit_hello_init(void);


rs_error * e = 0;
void check_error()
{
    if(e)
    {
        post("rs_error was raised when calling %s(%s):\n", rs_get_failed_function(e), rs_get_failed_args(e));
        post("    %s\n", rs_get_error_message(e));
        exit(EXIT_FAILURE);
    }
}

int init_realsense(void)
{

    /* Create a context object. This object owns the handles to all connected realsense devices. */
    rs_context * ctx = rs_create_context(RS_API_VERSION, &e);
    check_error();
    post("There are %d connected RealSense devices.\n", rs_get_device_count(ctx, &e));
    check_error();
    if(rs_get_device_count(ctx, &e) == 0) return EXIT_FAILURE;

    /* This tutorial will access only a single device, but it is trivial to extend to multiple devices */
    rs_device * dev = rs_get_device(ctx, 0, &e);
    check_error();
    post("\nUsing device 0, an %s\n", rs_get_device_name(dev, &e));
    check_error();
    post("    Serial number: %s\n", rs_get_device_serial(dev, &e));
    check_error();
    post("    Firmware version: %s\n", rs_get_device_firmware_version(dev, &e));
    check_error();

    /* Configure depth to run at VGA resolution at 30 frames per second */
    rs_enable_stream(dev, RS_STREAM_DEPTH, 640, 480, RS_FORMAT_Z16, 30, &e);
    check_error();
    rs_start_device(dev, &e);
    check_error();

    /* Determine depth value corresponding to one meter */
    const uint16_t one_meter = (uint16_t)(1.0f / rs_get_device_depth_scale(dev, &e));
    check_error();
}




void ext_main(void *r)
{
    init_realsense();
	void *p,*q,*attr;
	long attrflags;

	jit_hello_init();
    setup(&class_max_jit_hello, (method)max_jit_hello_new, (method)max_jit_hello_free, (short)sizeof(t_max_jit_hello),
		  0L, A_GIMME, 0);

	p = max_jit_classex_setup(calcoffset(t_max_jit_hello,obex));
	q = jit_class_findbyname(gensym("jit_hello"));
	max_jit_classex_standard_wrap(p,q,0);
	addmess((method)max_jit_hello_assist,		"assist",		A_CANT,0);
}

void max_jit_hello_assist(t_max_jit_hello *x, void *b, long m, long a, char *s)
{
	//nada for now
}

void max_jit_hello_free(t_max_jit_hello *x)
{
	jit_object_free(max_jit_obex_jitob_get(x));
	max_jit_obex_free(x);
}

void *max_jit_hello_new(t_symbol *s, long argc, t_atom *argv)
{
	t_max_jit_hello *x;
	long attrstart;
	t_symbol *text=gensym("Hello World!");
	void *o;

	if (x = (t_max_jit_hello *)max_jit_obex_new(class_max_jit_hello,gensym("jit_hello"))) {
		max_jit_obex_dumpout_set(x, outlet_new(x,0L)); //general purpose outlet(rightmost)

		//get normal args
		attrstart = max_jit_attr_args_offset(argc,argv);
		if (attrstart&&argv) {
			jit_atom_arg_getsym(&text, 0, attrstart, argv);
		}
		if (o=jit_object_new(gensym("jit_hello"),text)) {
			max_jit_obex_jitob_set(x,o);
		} else {
            freeobject((t_object *)x);
			x = NULL;
			jit_object_error((t_object *)x,"jit.hello: out of memory");
			goto out;
		}
	}

out:
	return (x);
}

}
