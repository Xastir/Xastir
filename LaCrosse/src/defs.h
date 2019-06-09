//
//
// Copyright (C) 2005 Jim Chandler <jim@n0vh.org>
// Portions Copyright (C) 2000-2019 The Xastir Group
//
//
// Dummy info here for now...
//


#define VERSION       "0.1.0"
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


