#pragma once
#include <jit.common.h>
#include <vector>
#include <array>
#include <cstdint>

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


template <typename Type, typename T>
static void add_attribute(std::string name, T Type::*member)
{
    const auto flags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    jit_class_addattr(Type::max_class,
                      (t_jit_object*) jit_object_new(
                          _jit_sym_jit_attr_offset,
                          name.c_str(),
                          _jit_sym_long,
                          flags,
                          (method)nullptr,
                          (method)nullptr,
                          get_offset(member)));
}

template <typename Type, typename T, typename Gadget>
static void add_output_attribute(std::string name, int num, T Gadget::*member)
{
    const auto flags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    const auto outputs_offset = get_offset(&Type::outputs, num);

    jit_class_addattr(Type::max_class,
                      (t_jit_object*) jit_object_new(
                          _jit_sym_jit_attr_offset,
                          name.c_str(),
                          _jit_sym_long,
                          flags,
                          (method)nullptr,
                          (method)nullptr,
                          outputs_offset + get_offset(member)));
}

template <typename Type, typename T, typename Gadget>
static void add_array_output_attribute(std::string name, int num, T Gadget::*member)
{
    const auto flags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    const auto outputs_offset = get_offset(&Type::outputs, num);

    jit_class_addattr(Type::max_class,
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
