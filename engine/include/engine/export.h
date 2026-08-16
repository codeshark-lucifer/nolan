#pragma once

#ifdef _WIN32

#ifdef OLIA_BUILD
#define OLIA_API __declspec(dllexport)
#else
#define OLIA_API __declspec(dllimport)
#endif

#else

#define OLIA_API

#endif