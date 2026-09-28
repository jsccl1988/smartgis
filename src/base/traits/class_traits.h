// Copyright (c) 2018 The Mogu Authors.
// All rights reserved.
// Inspire from facebook folly

#ifndef BASE_TRAITS_CLASS_TRAITS_H
#define BASE_TRAITS_CLASS_TRAITS_H

#include <cstdint>
#include <functional>
#include <type_traits>

#include "base/traits/concept.h"
#include "base/traits/is_detected.h"

namespace base {
// class layout
namespace class_layout {
template <class T>
constexpr void *method_address(T &&p) {
  union U {
    T t;
    void *address;
  };
  return ((U *)&p)->address;
}

template <class T>
constexpr auto vtable_offset(T &&p) {
  union U {
    T t;
    int64_t offset;
  };
  return ((U *)&p)->offset - 1;
}

constexpr void *vmethod_address(void *obj, int offset) {
  return *(void **)(*(intptr_t *)obj + offset);
}
}  // namespace class_layout
}  // namespace base

// member traits
#define DEFINE_MEMBER_TRAITS_IMPL(member)                                \
  template <class, class = std::void_t<>>                                \
  struct has_member_##member : std::false_type {};                       \
  template <class Type>                                                  \
  struct has_member_##member<Type, std::void_t<decltype(&Type::member)>> \
      : std::true_type {}

#define DEFINE_NESTED_TYPE_TRAITS_IMPL(member)                              \
  template <class, class = std::void_t<>>                                   \
  struct has_nested_type_##member : std::false_type {};                     \
  template <class Type>                                                     \
  struct has_nested_type_##member<Type, std::void_t<typename Type::member>> \
      : std::true_type {}

#define DEFINE_NON_STATIC_FUNCTION_TRAITS_IMPL(member)                 \
  template <class, class = std::void_t<>>                              \
  struct has_non_static_function_##member : std::false_type {};        \
  template <class Type>                                                \
  struct has_non_static_function_##member<                             \
      Type, std::enable_if_t<std::is_member_function_pointer<decltype( \
                &Type::member)>::value>> : std::true_type {}

#define DEFINE_STATIC_FUNCTION_TRAITS_IMPL(member)                             \
  template <class, class = std::void_t<>>                                      \
  struct has_static_function_##member : std::false_type {};                    \
  template <class Type>                                                        \
  struct has_static_function_##member<                                         \
      Type,                                                                    \
      std::enable_if_t<(!has_non_static_function_##member<Type>::value) &&     \
                       std::is_function<typename std::remove_pointer<decltype( \
                           &Type::member)>::type>::value>> : std::true_type {}

#define DEFINE_NON_FUNCTION_MEMBER_TRAITS_IMPL(member)                      \
  template <class, class = std::void_t<>>                                   \
  struct has_non_function_member_##member : std::false_type {};             \
  template <class Type>                                                     \
  struct has_non_function_member_##member<                                  \
      Type,                                                                 \
      std::enable_if_t<has_nested_type_##member<Type>::value ||             \
                       (has_member_##member<Type>::value &&                 \
                        (!has_non_static_function_##member<Type>::value) && \
                        (!has_static_function_##member<Type>::value))>>     \
      : std::true_type {}

#define DEFINE_MEMBER_TRAITS(member)              \
  DEFINE_MEMBER_TRAITS_IMPL(member);              \
  DEFINE_NESTED_TYPE_TRAITS_IMPL(member);         \
  DEFINE_NON_STATIC_FUNCTION_TRAITS_IMPL(member); \
  DEFINE_STATIC_FUNCTION_TRAITS_IMPL(member);     \
  DEFINE_NON_FUNCTION_MEMBER_TRAITS_IMPL(member)

#define DEFINE_METHOD_TRAITS_IMPL(traits, func, cv_qualify)   \
  template <typename Type, typename Return, typename... Args> \
  struct traits##__impl__<Type, Return(Args...) cv_qualify> { \
    template <typename U, Return (U::*)(Args...) cv_qualify>  \
    struct sfinae {};                                         \
    template <typename U>                                     \
    static std::true_type has_##func(sfinae<U, &U::func> *);  \
    template <typename>                                       \
    static std::false_type has_##func(...);                   \
  }

#define DEFINE_METHOD_TRAITS(traits, func)                 \
  template <typename, typename>                            \
  struct traits##__impl__;                                 \
  DEFINE_METHOD_TRAITS_IMPL(traits, func, );               \
  DEFINE_METHOD_TRAITS_IMPL(traits, func, const);          \
  DEFINE_METHOD_TRAITS_IMPL(traits, func, volatile);       \
  DEFINE_METHOD_TRAITS_IMPL(traits, func, const volatile); \
  template <typename Type, typename Signature>             \
  using traits = decltype(                                 \
      traits##__impl__<Type, Signature>::template has_##func<Type>(nullptr))

namespace base {
template <typename T, typename R, template <class...> class M, typename... Args>
constexpr bool has_method = M<T, R, Args...>::template tv<T>::value;

template <typename T, template <class...> class M, typename V>
constexpr bool has_member = identical_to<V, M, T>;
}  // namespace base

#define METHOD_TRAIT(trait_name, method_name)                                  \
  template <class T, typename R, typename... Args>                             \
  struct trait_name {                                                          \
    template <typename T_>                                                     \
    static constexpr bool is_const =                                           \
        not std::is_same_v<std::remove_const_t<T_>, T_>;                       \
                                                                               \
    template <typename T_, typename = int>                                     \
    struct fptr_meta {                                                         \
      template <typename... Args_>                                             \
      using type = typename std::integral_constant<                            \
          decltype(std::declval<T_>().method_name(std::declval<Args_>()...)) ( \
              T_::*)(Args_...),                                                \
          &T_::method_name>::value_type;                                       \
    };                                                                         \
                                                                               \
    template <typename T_>                                                     \
    struct fptr_meta<T_, std::enable_if_t<is_const<T_>, int>> {                \
      template <typename... Args_>                                             \
      using type = typename std::integral_constant<                            \
          decltype(std::declval<T_>().method_name(std::declval<Args_>()...)) ( \
              T_::*)(Args_...) const,                                          \
          &T_::method_name>::value_type;                                       \
    };                                                                         \
                                                                               \
    template <typename T_, typename... Args_>                                  \
    using fptr_meta_t = typename fptr_meta<T_>::template type<Args_...>;       \
                                                                               \
    template <typename T_, typename... Args_>                                  \
    using qual_ret =                                                           \
        decltype(std::declval<T_>().method_name(std::declval<Args_>()...));    \
                                                                               \
    template <typename T_, typename = int>                                     \
    struct tv {                                                                \
      static constexpr bool value = false;                                     \
    };                                                                         \
    template <typename T_>                                                     \
    struct tv<T_,                                                              \
              std::enable_if_t<                                                \
                  is_detected_exact<R, qual_ret, T_, Args...>::value, int>> {  \
      static constexpr bool value =                                            \
          is_detected<fptr_meta_t, T, Args...>::value;                         \
    };                                                                         \
  }
#endif  // BASE_TRAITS_CLASS_TRAITS_H
