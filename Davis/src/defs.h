
//
// Copyright (C) 2004 Bruce Bennett <bruts@adelphia.net>
// Portions Copyright (C) 2000-2023 The Xastir Group
//
//
// Dummy info here for now...
//


#define PORT          1313
#define CONNECTIONS   20
#define NOFILE        20


// Defines we can use to mark functions and parameters as "unused" to the compiler
#ifdef __GNUC__
  #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
  #define UNUSED(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
  #define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
  #define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif


