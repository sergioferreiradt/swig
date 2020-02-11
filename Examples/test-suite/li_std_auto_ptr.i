%module li_std_auto_ptr

%{
#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // auto_ptr deprecation
#endif

#if defined(__clang__)
#pragma clang diagnostic push
// Suppress 'auto_ptr<>' is deprecated
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
%}

#if defined(SWIGCSHARP) || defined(SWIGJAVA) || defined(SWIGPYTHON) || defined(SWIGJAVASCRIPT)

// Prefer using std::unique_ptr<> if it's available, as auto_ptr<> generates
// deprecation warnings with C++14 compilers and may not exist at all when
// using C++17.
#if __cplusplus >= 201103L
#define SWIG_AUTO_PTR_CLASSNAME unique_ptr
#endif

%include "std_auto_ptr.i"

%auto_ptr(Klass)

%{
#include <memory>
#include <string>
#include "swig_examples_lock.h"
%}

%inline %{

class Klass {
public:
  explicit Klass(const char* label) :
    m_label(label)
  {
    SwigExamples::Lock lock(critical_section);
    total_count++;
  }

  const char* getLabel() const { return m_label.c_str(); }

  ~Klass()
  {
    SwigExamples::Lock lock(critical_section);
    total_count--;
  }

  static int getTotal_count() { return total_count; }

private:
  static SwigExamples::CriticalSection critical_section;
  static int total_count;

  std::string m_label;
};

SwigExamples::CriticalSection Klass::critical_section;
int Klass::total_count = 0;

%}

%template(KlassAutoPtr) std::auto_ptr<Klass>;

%inline %{

std::auto_ptr<Klass> makeKlassAutoPtr(const char* label) {
  return std::auto_ptr<Klass>(new Klass(label));
}

%}

#endif
