/* -----------------------------------------------------------------------------
 * This file is part of SWIG, which is licensed as a whole under version 3
 * (or any later version) of the GNU General Public License. Some additional
 * terms also apply to certain portions of SWIG. The full details of the SWIG
 * license and copyrights can be found in the LICENSE and COPYRIGHT files
 * included with the SWIG source code as distributed by the SWIG developers
 * and at http://www.swig.org/legal.html.
 *
 * javascript.cxx
 *
 * Javascript language module for SWIG.
 * ----------------------------------------------------------------------------- */

#include <ctype.h>
#include "swigmod.h"
#include "cparse.h"

/**
 * Enables extra debugging information in typemaps.
 */
static bool js_template_enable_debug = false;

#define ERR_MSG_ONLY_ONE_ENGINE_PLEASE "Only one engine can be specified at a time."

// keywords used for state variables
#define NAME "name"
#define NAME_MANGLED "name_mangled"
#define TYPE "type"
#define TYPE_MANGLED "type_mangled"
#define WRAPPER_NAME "wrapper"
#define IS_IMMUTABLE "is_immutable"
#define IS_STATIC "is_static"
#define IS_ABSTRACT "is_abstract"
#define GETTER "getter"
#define SETTER "setter"
#define PARENT "parent"
#define PARENT_MANGLED "parent_mangled"
#define CTOR "ctor"
#define CTOR_WRAPPEbaseClassName \
  = NULL;                        \
  RS "ctor_wrappers"
#define CTOR_DISPATCHERS "ctor_dispatchers"
#define DTOR "dtor"
#define ARGCOUNT "wrap:argc"
#define HAS_TEMPLATES "has_templates"
#define FORCE_CPP "force_cpp"
#define RESET true

// keys for global state variables
#define CREATE_NAMESPACES "create_namespaces"
#define REGISTER_NAMESPACES "register_namespaces"
#define INITIALIZER "initializer"

// keys for class scoped state variables
#define MEMBER_VARIABLES "member_variables"
#define MEMBER_FUNCTIONS "member_functions"
#define STATIC_FUNCTIONS "static_functions"
#define STATIC_VARIABLES "static_variables"

/**
 * A convenience class to manage state variables for emitters.
 * The implementation delegates to SWIG Hash DOHs and provides
 * named sub-hashes for class, variable, and function states.
 */
class JSEmitterState
{

public:
  JSEmitterState();
  ~JSEmitterState();
  DOH *globals();
  DOH *globals(const char *key, DOH *initial = 0);
  DOH *clazz(bool reset = false);
  DOH *clazz(const char *key, DOH *initial = 0);
  DOH *function(bool reset = false);
  DOH *function(const char *key, DOH *initial = 0);
  DOH *variable(bool reset = false);
  DOH *variable(const char *key, DOH *initial = 0);
  static int IsSet(DOH *val);

private:
  DOH *getState(const char *key, bool reset = false);
  Hash *globalHash;
};

/**
 * A convenience class that wraps a code snippet used as template
 * for code generation.typemapLookup(n, "typescriptbase", typemapLookupCppType, WARN_NONE);
 */
class Template
{

public:
  Template(const String *code);
  Template(const String *code, const String *templateName);
  Template(const Template &other);
  ~Template();
  String *str();
  Template &replace(const String *pattern, const String *repl);
  Template &print(DOH *doh);
  Template &pretty_print(DOH *doh);
  void operator=(const Template &t);
  Template &trim();

private:
  String *code;
  String *templateName;
};

/* -----------------------------------------------------------------------------
 * Typescript Proxy emitter helper
 * ----------------------------------------------------------------------------- */
class ProxyInterface
{
public:
  enum ProxyType
  {
    classType,
    interfaceType,
    enumType
  };

  ProxyInterface(ProxyType mProxyType) : proxyType(mProxyType), empty_string(NewString(""))
  {
    classCode = NewString("");
    proxyExtraCode = NewString("");
    baseClassName = NewString("");
    printf("Class code variable was initialized\n");
  };
  ~ProxyInterface();
  void generateProxy();
  void setClassName(String *name) { className = NewString(name); }
  void addMemberVariable(Node *n, String *typescriptType);
  void addEnumValue(Node *n, String *value);
  void insertCode(String *code);

  // Node *n;
  String *baseClassName;

private:
  String *classFileName;
  String *classFilePath;
  String *className;
  String *classCode;
  String *proxyExtraCode;
  File *classFilePtr;
  ProxyType proxyType;
  String *typemapLookup(Node *n, const_String_or_char_ptr tmap_method, SwigType *type, int warning, Node *typemap_attributes = 0);
  String *getProxyName(SwigType *t, bool jnidescriptor = false);
  const String *empty_string;
};

/**
 * JSEmitter represents an abstraction of javascript code generators
 * for different javascript engines.
 **/
class JSEmitter
{

protected:
  typedef JSEmitterState State;

  enum MarshallingMode
  {
    Setter,
    Getter,
    Ctor,
    Function
  };

public:
  enum JSEngine
  {
    JavascriptCore,
    V8,
    NodeJS
  };

  JSEmitter(JSEngine engine);

  virtual ~JSEmitter();

  /**
   * Opens output files and temporary output DOHs.
   */
  virtual int initialize(Node *n);

  /**
   * Writes all collected code into the output file(s).
   */
  virtual int dump(Node *n) = 0;

  /**
   * Cleans up all open output DOHs.
   */
  virtual int close() = 0;

  /**
   * Switches the context for code generation.
   *
   * Classes, global variables and global functions may need to
   * be registered in certain static tables.
   * This method should be used to switch output DOHs correspondingly.
   */
  virtual int switchNamespace(Node *);

  /**
   * Invoked at the beginning of the classHandler.
   */
  virtual int enterClass(Node *);

  /**
   * Invoked at the end of the classHandler.
   */
  virtual int exitClass(Node *)
  {
    return SWIG_OK;
  };

  /**
   * Invoked at the beginning of the variableHandler.
   */
  virtual int enterVariable(Node *);

  /**
   * Invoked at the end of the variableHandler.
   */
  virtual int exitVariable(Node *)
  {
    return SWIG_OK;
  };

  /**
   * Invoked at the beginning of the functionHandler.
   */
  virtual int enterFunction(Node *);

  /**
   * Invoked at the end of the functionHandler.
   */
  virtual int exitFunction(Node *)
  {
    return SWIG_OK;
  };

  /**
   * Invoked by functionWrapper callback after call to Language::functionWrapper.
   */
  virtual int emitWrapperFunction(Node *n);

  /**
   * Invoked by nativeWrapper callback
   */
  virtual int emitNativeFunction(Node *n);

  /**
   * Invoked from constantWrapper after call to Language::constantWrapper.
   **/
  virtual int emitConstant(Node *n);

  /**
   * Registers a given code snippet for a given key name.
   *
   * This method is called by the fragmentDirective handler
   * of the JAVASCRIPT language module.
   **/
  int registerTemplate(const String *name, const String *code);

  /**
   * Retrieve the code template registered for a given name.
   */
  Template getTemplate(const String *name);

  State &getState();

protected:
  /**
   * Generates code for a constructor function.
   */
  virtual int emitCtor(Node *n);

  /**
   * Generates code for a destructor function.
   */
  virtual int emitDtor(Node *n);

  /**
   * Generates code for a function.
   */
  virtual int emitFunction(Node *n, bool is_member, bool is_static);

  virtual int emitFunctionDispatcher(Node *n, bool /*is_member */);

  /**
   * Generates code for a getter function.
   */
  virtual int emitGetter(Node *n, bool is_member, bool is_static);

  /**
   * Generates code for a setter function.
   */
  virtual int emitSetter(Node *n, bool is_member, bool is_static);

  virtual void marshalInputArgs(Node *n, ParmList *parms, Wrapper *wrapper, MarshallingMode mode, bool is_member, bool is_static) = 0;

  virtual String *emitInputTypemap(Node *n, Parm *params, Wrapper *wrapper, String *arg);

  virtual void marshalOutput(Node *n, ParmList *params, Wrapper *wrapper, String *actioncode, const String *cresult = 0, bool emitReturnVariable = true);

  virtual void emitCleanupCode(Node *n, Wrapper *wrapper, ParmList *params);

  /**
   * Helper function to retrieve the first parent class node.
   */
  Node *getBaseClass(Node *n);

  Parm *skipIgnoredArgs(Parm *p);

  virtual int createNamespace(String *scope);

  virtual Hash *createNamespaceEntry(const char *name, const char *parent, const char *parent_mangled);

  virtual int emitNamespaces() = 0;

protected:
  JSEngine engine;
  Hash *templates;
  State state;

  // contains context specific data (DOHs)
  // to allow generation of namespace related code
  // which are switched on namespace change
  Hash *namespaces;
  Hash *current_namespace;
  String *defaultResultName;
  File *f_wrappers;
};

/* factory methods for concrete JSEmitters: */

JSEmitter *swig_javascript_create_JSCEmitter();
JSEmitter *swig_javascript_create_V8Emitter();
JSEmitter *swig_javascript_create_NodeJSEmitter();

/**********************************************************************
 * JAVASCRIPT: SWIG module implementation
 **********************************************************************/

class JAVASCRIPT : public Language
{

public:
  JAVASCRIPT() : emitter(NULL), empty_string(NewString("")),
                 imclass_cppcasts_code(NULL), upcasts_code(NULL),
                 proxy_class_name(NULL)
  {
  }
  ~JAVASCRIPT()
  {
    delete emitter;
  }

  virtual int functionHandler(Node *n);
  virtual int globalfunctionHandler(Node *n);
  virtual int variableHandler(Node *n);
  virtual int globalvariableHandler(Node *n);
  virtual int staticmemberfunctionHandler(Node *n);
  virtual int membervariableHandler(Node *n);
  virtual int classHandler(Node *n);
  virtual int enumDeclaration(Node *n);
  virtual int enumvalueDeclaration(Node *n);
  virtual int insertDirective(Node *n);
  virtual int functionWrapper(Node *n);
  virtual int constantWrapper(Node *n);
  virtual int nativeWrapper(Node *n);
  virtual void main(int argc, char *argv[]);
  virtual int top(Node *n);

  /**
   *  Registers all %fragments assigned to section "templates".
   **/
  virtual int fragmentDirective(Node *n);

  ProxyInterface *proxyInterface;
  ProxyInterface *proxyEnum;

public:
  virtual String *getNSpace() const;

private:
  String *empty_string;
  // TODO : The next two variables allocation need to be fixed
  String *proxy_class_def;
  String *imclass_cppcasts_code; //C++ casts up inheritance hierarchies intermediary class code
  String *upcasts_code;          //C++ casts for inheritance hierarchies C++ code
  String *proxy_class_name;      // proxy class name

  JSEmitter *emitter;
  String *getBaseClass(Node *n);
  String *getProxyName(SwigType *t, bool jnidescriptor = false);
  String *typemapLookup(Node *n, const_String_or_char_ptr tmap_method, SwigType *type, int warning, Node *typemap_attributes = 0);
  String *getTypescriptProxyType(Node *n);
  String *getEnumName(SwigType *t, bool jnidescriptor = false);
  void generateBaseInterfaces(Node *topNode);
  void substituteClassnameSpecialVariable(SwigType *classnametype, String *tm, const char *classnamespecialvariable, bool jnidescriptor = false);
  Node *findTemplate(Node *topNode, char *name);
  Node *findInsert(Node *topNode, char *section);
  bool isTemplate(Node *n, char *name)
  {
    if (Strcmp(nodeType(n), "template") == 0 && Strcmp(Getattr(n, "name"), name) == 0)
      return true;
    return false;
  }

  bool isInsert(Node *n, char *section)
  {
    if (Strcmp(nodeType(n), "insert") == 0 && Strcmp(Getattr(n, "section"), section) == 0)
      return true;
    return false;
  }
};

/* -----------------------------------------------------------------------------
 * Swig_string_kcase()
 *
 * Convert Pascal Case Swig String to Kebab Case
 *
 *      CamelCase -> camel-case
 *      get2D     -> get-2d
 *      asFloat2  -> as-float2
 * ----------------------------------------------------------------------------- */

String *Swig_string_kcase(String *s)
{
  String *ns;
  int c;
  int lastC = 0;
  int nextC = 0;
  int underscore = 0;
  ns = NewStringEmpty();

  /* We insert a dash when:
     1. Lower case char followed by upper case char
     getFoo > get_foo; getFOo > get_foo; GETFOO > getfoo
     2. Number preceded by char and not end of string
     get2D > get_2d; get22D > get_22d; GET2D > get_2d
     but:
     asFloat2 > as_float2
  */

  Seek(s, 0, SEEK_SET);

  while ((c = Getc(s)) != EOF)
  {
    nextC = Getc(s);
    Ungetc(nextC, s);
    if (isdigit(c) && isalpha(lastC) && nextC != EOF)
      underscore = 1;
    else if (isupper(c) && isalpha(lastC) && !isupper(lastC))
      underscore = 1;

    lastC = c;

    if (underscore)
    {
      Putc('-', ns);
      underscore = 0;
    }

    Putc(tolower(c), ns);
  }
  return ns;
}

/* ---------------------------------------------------------------------
 * functionWrapper()
 *
 * Low level code generator for functions
 * --------------------------------------------------------------------- */

int JAVASCRIPT::functionWrapper(Node *n)
{

  // note: the default implementation only prints a message
  // Language::functionWrapper(n);
  emitter->emitWrapperFunction(n);

  return SWIG_OK;
}

/* ---------------------------------------------------------------------
 * functionHandler()
 *
 * Function handler for generating wrappers for functions
 * --------------------------------------------------------------------- */
int JAVASCRIPT::functionHandler(Node *n)
{

  if (GetFlag(n, "isextension") == 1)
  {
    SetFlag(n, "ismember");
  }

  emitter->enterFunction(n);
  Language::functionHandler(n);
  emitter->exitFunction(n);

  return SWIG_OK;
}

/* ---------------------------------------------------------------------
 * globalfunctionHandler()
 *
 * Function handler for generating wrappers for functions
 * --------------------------------------------------------------------- */

int JAVASCRIPT::globalfunctionHandler(Node *n)
{
  emitter->switchNamespace(n);
  Language::globalfunctionHandler(n);

  return SWIG_OK;
}

/* ---------------------------------------------------------------------
 * staticmemberfunctionHandler()
 *
 * Function handler for generating wrappers for static member functions
 * --------------------------------------------------------------------- */

int JAVASCRIPT::staticmemberfunctionHandler(Node *n)
{
  /*
   *  Note: storage=static is removed by Language::staticmemberfunctionHandler.
   *    So, don't rely on that after here. Instead use the state variable which is
   *    set by JSEmitter::enterFunction().
   */
  Language::staticmemberfunctionHandler(n);
  return SWIG_OK;
}

/* ---------------------------------------------------------------------
 * variableHandler()
 *
 * Function handler for generating wrappers for variables
 * --------------------------------------------------------------------- */

int JAVASCRIPT::variableHandler(Node *n)
{

  emitter->enterVariable(n);
  Language::variableHandler(n);
  emitter->exitVariable(n);

  return SWIG_OK;
}

/* ---------------------------------------------------------------------
 * globalvariableHandler()
 *
 * Function handler for generating wrappers for global variables
 * --------------------------------------------------------------------- */

int JAVASCRIPT::globalvariableHandler(Node *n)
{
  emitter->switchNamespace(n);
  Language::globalvariableHandler(n);

  return SWIG_OK;
}

/* ---------------------------------------------------------------------
 * constantHandler()
 *
 * Function handler for generating wrappers for constants
 * --------------------------------------------------------------------- */

int JAVASCRIPT::constantWrapper(Node *n)
{
  emitter->switchNamespace(n);

  // Note: callbacks trigger this wrapper handler
  // TODO: handle callback declarations
  if (Equal(Getattr(n, "kind"), "function"))
  {
    return SWIG_OK;
  }
  // TODO: the emitter for constants must be implemented in a cleaner way
  // currently we treat it like a read-only variable
  // however, there is a remaining bug with function pointer constants
  // which could be fixed with a cleaner approach
  emitter->emitConstant(n);

  return SWIG_OK;
}

/* ---------------------------------------------------------------------
 * nativeWrapper()
 *
 * Function wrapper for generating placeholders for native functions
 * --------------------------------------------------------------------- */

int JAVASCRIPT::nativeWrapper(Node *n)
{
  emitter->emitNativeFunction(n);

  return SWIG_OK;
}

/**
 * Function handler for generating wrappers for class
 * When top() is called to traverse the node tree, this function is called
 * id the node is "class" type
 */
int JAVASCRIPT::classHandler(Node *n)
{
  emitter->switchNamespace(n);
  emitter->enterClass(n);

  proxyInterface = new ProxyInterface(ProxyInterface::interfaceType);
  proxyInterface->setClassName(Getattr(n, "sym:name"));

  // Calling default swig handler to continue normal tree traverse
  Language::classHandler(n);

  emitter->exitClass(n);

  proxyInterface->baseClassName = getBaseClass(n);
  proxyInterface->generateProxy();
  delete proxyInterface;
  proxyInterface = NULL;

  return SWIG_OK;
}

Node *JAVASCRIPT::findInsert(Node *topNode, char *section)
{
  if (!topNode)
  {
    return NULL;
  }

  if (isInsert(topNode, section))
  {
    Printf(stdout, " FOUND Insert %s ================================================= 26\n", section);
    Swig_print_node(topNode);
    Printf(stdout, "Code : %s\n", Getattr(topNode, "code"));
    Printf(stdout, "================================================= 26\n");
    return topNode;
  }

  Node *currentNode = firstChild(topNode);
  Node *returnNode = 0;
  while (currentNode)
  {
    returnNode = findInsert(currentNode, section);
    if (returnNode)
    {
      return returnNode;
    }
    currentNode = nextSibling(currentNode);
  }
  return NULL;
}

void JAVASCRIPT::generateBaseInterfaces(Node *topNode)
{
  proxyInterface = new ProxyInterface(ProxyInterface::interfaceType);
  String *className = NewString("vector");
  proxyInterface->setClassName(className);
  Delete(className);
  Printf(stdout, "================================================================ 25\n");
  // SwigType *baseclassname = NewString("vector<(long)>");
  // Node *n = classLookup(baseclassname);
  // if (n) {
  Swig_print_node(topNode);
  Printf(stdout, "================================================================ 24\n");
  // Find a template called vector
  Node *templateNode = findTemplate(topNode, "vector");
  // TODO : Protect the code with if
  Node *insertCodeNode = findInsert(templateNode, "proxycode");
  Printf(stdout, "Code : %s\n", Getattr(insertCodeNode, "code"));
  proxyInterface->insertCode(Getattr(insertCodeNode, "code"));
  // }
  proxyInterface->generateProxy();
  delete proxyInterface;
  proxyInterface = NULL;

  Printf(stdout, "================================================================ 25\n");
}

/**
 * Try to find a template by name
 */
Node *JAVASCRIPT::findTemplate(Node *topNode, char *name)
{
  if (!topNode)
  {
    return NULL;
  }

  if (isTemplate(topNode, name))
  {
    Printf(stdout, " FOUND Template %s ================================================= 26\n", name);
    Swig_print_node(topNode);
    Printf(stdout, "================================================= 26\n");
    return topNode;
  }

  Node *currentNode = firstChild(topNode);
  Node *returnNode = 0;
  while (currentNode)
  {
    returnNode = findTemplate(currentNode, name);
    if (returnNode)
    {
      return returnNode;
    }
    currentNode = nextSibling(currentNode);
  }
  return NULL;
}

/**
 * Executed by SWIG to manage a declared ++ public variable
 *
 * @param n The node representing the public variable
 * @return status to swig decide what to do
 */
int JAVASCRIPT::membervariableHandler(Node *n)
{
  String *typescriptProxyType = getTypescriptProxyType(n);
  proxyInterface->addMemberVariable(n, typescriptProxyType);
  Language::membervariableHandler(n);
  return SWIG_OK;
}

/**
 * Executed for each enum
 *
 * @param n The node that represents the enumerated
 */
int JAVASCRIPT::enumDeclaration(Node *n)
{
  Printf(stdout, "###### 20 1 - Enum : %s\n ", Getattr(n, "sym:name"));
  proxyEnum = new ProxyInterface(ProxyInterface::enumType);
  // proxyEnum->n = n;
  proxyEnum->setClassName(Getattr(n, "sym:name"));
  Language::enumDeclaration(n);
  proxyEnum->generateProxy();
  delete proxyEnum;
  proxyEnum = NULL;
  Printf(stdout, "###### 20 3 - Enum : %s\n ", Getattr(n, "sym:name"));
  return SWIG_OK;
}

int JAVASCRIPT::enumvalueDeclaration(Node *n)
{
  Printf(stdout, "###### 20 2 - Enum declaration : %s\n ", Getattr(n, "sym:name"));
  Printf(stdout, "###### 20 2 - Enum Value : %s\n", Getattr(n, "enumvalue"));
  Printf(stdout, "###### 20 2 - Just Value : %s\n", Getattr(n, "value"));

  if (proxyEnum)
  {
    proxyEnum->addEnumValue(n, Getattr(n, "enumvalue"));
  }
  return Language::enumvalueDeclaration(n);
}

/**
 * Executed when a directive is inserted
 * Check if the insertion is to be done as "proxycode" and if a class is
 * being handled (classHandler executed) and if so, store the code in the
 * object representation of the proxy
 * If not, execute the standard Language insertDirective
 *
 * @param n The node representing the code
 * @return SWIG result status
 */
int JAVASCRIPT::insertDirective(Node *n)
{
  String *code = Getattr(n, "code");
  String *section = Getattr(n, "section");

  if (proxyInterface && Cmp(section, "proxycode") == 0)
  {
    proxyInterface->insertCode(code);
  }
  else
  {
    return Language::insertDirective(n);
  }
  return SWIG_OK;
}

/**
 * Obtain the information about the base class that a node extends
 *
 * @param n The node that represents a class
 * @return the string representation of the base class name
 */
String *JAVASCRIPT::getBaseClass(Node *n)
{
  SwigType *c_baseclassname = NULL;
  String *returnBaseClassName = NULL;

  // Check if for this class (represented by the node), there is a typemap defining
  // the class (in the target typescript language) that should be used as superclass
  String *typemapLookupCppType = Getattr(n, "name");
  Printf(stdout, "##### 9 - CPP Base Class %s\n", typemapLookupCppType);
  String *typescriptTypemapBaseClassName = typemapLookup(n, "typescriptbase", typemapLookupCppType, WARN_NONE);
  if (typescriptTypemapBaseClassName && Strcmp(typescriptTypemapBaseClassName, "") != 0)
  {
    Printf(stdout, "##### 9 - Base Class From typescriptbase : <%s>\n", typescriptTypemapBaseClassName);
    return typescriptTypemapBaseClassName;
  }

  // Get the list of attributes of the node that contain
  // several flavours of the classname
  List *baselist = Getattr(n, "bases");
  if (baselist)
  {
    Iterator base = First(baselist);
    while (base.item)
    {
      if (!(GetFlag(base.item, "feature:ignore") || Getattr(base.item, "feature:interface")))
      {
        SwigType *baseclassname = Getattr(base.item, "name");
        Printf(stdout, " ####### Base Classname : %s\n", baseclassname);
        returnBaseClassName = NewString(baseclassname);
        if (!c_baseclassname)
        {
          Printf(stdout, " ####### 15 Base Classname before : %s\n", baseclassname);
          String *name = getProxyName(baseclassname);
          Printf(stdout, " ####### 15 Base Classname after : %s\n", name);
          if (name)
          {
            c_baseclassname = baseclassname;
            returnBaseClassName = name;
          }
        }
        else
        {
          /* Warn about multiple inheritance for additional base class(es) */
          String *proxyclassname = Getattr(n, "classtypeobj");
          Swig_warning(WARN_JAVA_MULTIPLE_INHERITANCE, Getfile(n), Getline(n),
                       "Warning for %s, base %s ignored. Multiple inheritance is not supported in Java.\n", SwigType_namestr(proxyclassname), SwigType_namestr(baseclassname));
        }
      }
      base = Next(base);
    }
  }
  else
  {
    Printf(stdout, "------------------------------------- NO BASE CLASS FOR %s\n", Getattr(n, "sym:name"));
    String *typemap_lookup_type = Getattr(n, "name");
    Printf(stdout, "-------------- Typemap lookup type : %s\n", typemap_lookup_type);
    const String *pure_baseclass = typemapLookup(n, "typescriptbase", typemap_lookup_type, WARN_NONE);
    Printf(stdout, "-------------- Pure Base Class : %s\n", pure_baseclass);
  }
  Printf(stdout, "##### 9 - Base Class : %s for %s\n", returnBaseClassName, Getattr(n, "sym:name"));
  return returnBaseClassName;
}

/* -----------------------------------------------------------------------------
   * getProxyName()
   *
   * Test to see if a type corresponds to something wrapped with a proxy class.
   * Return NULL if not otherwise the proxy class name, fully qualified with
   * package name if the nspace feature is used, unless jnidescriptor is true as
   * the package name is handled differently (unfortunately for legacy reasons).
   * ----------------------------------------------------------------------------- */

String *JAVASCRIPT::getProxyName(SwigType *t, bool jnidescriptor)
{
  String *proxyname = NULL;
  // To remove
  String *package = NULL;

  // This node should be received as parameter because classLookup() exist in Language class
  Node *n = classLookup(t);
  if (!n)
  {
    n = enumLookup(t);
    if (n)
    {
      Printf(stdout, "##### 23 - Enum type Found : %s\n", Getattr(n, "name"));
    }
    else
    {
      Printf(stdout, "#### 23 - Enum not found for %s\n", Getattr(t, "name"));
    }
  }
  if (n)
  {
    Printf(stdout, "###### 10 - Class looked up : %s\n", Getattr(n, "name"));
    proxyname = Getattr(n, "proxyname");
    if (!proxyname || jnidescriptor)
    {
      String *nspace = Getattr(n, "sym:nspace");
      String *symname = Copy(Getattr(n, "sym:name"));
      if (symname && !GetFlag(n, "feature:flatnested"))
      {
        for (Node *outer_class = Getattr(n, "nested:outer"); outer_class; outer_class = Getattr(outer_class, "nested:outer"))
        {
          if (String *name = Getattr(outer_class, "sym:name"))
          {
            Push(symname, jnidescriptor ? "$" : ".");
            Push(symname, name);
          }
          else
            return NULL;
        }
      }
      if (nspace)
      {
        if (package && !jnidescriptor)
          proxyname = NewStringf("%s.%s.%s", package, nspace, symname);
        else
          proxyname = NewStringf("%s.%s", nspace, symname);
      }
      else
      {
        proxyname = Copy(symname);
      }
      if (!jnidescriptor)
      {
        Setattr(n, "proxyname", proxyname); // Cache it
        Delete(proxyname);
      }
      Delete(symname);
    }
  }
  Printf(stdout, "###### 8 : Proxy name : %s\n", proxyname);
  return proxyname;
}

/* -----------------------------------------------------------------------------
   * typemapLookup()
   * n - for input only and must contain info for Getfile(n) and Getline(n) to work
   * tmap_method - typemap method name
   * type - typemap type to lookup
   * warning - warning number to issue if no typemaps found
   * typemap_attributes - the typemap attributes are attached to this node and will
   *   also be used for temporary storage if non null
   * return is never NULL, unlike Swig_typemap_lookup()
   * ----------------------------------------------------------------------------- */

String *JAVASCRIPT::typemapLookup(Node *n, const_String_or_char_ptr tmap_method, SwigType *type, int warning, Node *typemap_attributes)
{
  Node *node = !typemap_attributes ? NewHash() : typemap_attributes;
  Setattr(node, "type", type);
  Setfile(node, Getfile(n));
  Setline(node, Getline(n));
  Printf(stdout, "###### 1 - Trying to find typemap %s for %s on %s\n", tmap_method, type, Getattr(n, "sym:name"));
  String *tm = Swig_typemap_lookup(tmap_method, node, "", 0);
  if (!tm)
  {
    tm = empty_string;
    if (warning != WARN_NONE)
      Swig_warning(warning, Getfile(n), Getline(n), "No %s typemap defined for %s\n", tmap_method, SwigType_str(type, 0));
    Printf(stdout, "###### 1 - NOT Found typemap for %s : %s\n", tmap_method, tm);
  }
  else
  {
    Printf(stdout, "###### 1 - Found typemap for %s : %s\n", tmap_method, tm);
  }
  if (!typemap_attributes)
    Delete(node);
  Printf(stdout, "###### 1 - Returned typemap for %s : %s\n", tmap_method, tm);
  return tm;
}

/**
 * Obtain the type that should be used in the TypeScript proxy declaration
 *
 * @param n The node representing C++ variable
 * @return A pointer to a string containing the type that should be used
 * verbatim in the TypeScript proxy
 */
String *JAVASCRIPT::getTypescriptProxyType(Node *n)
{
  // Obtain Proxy Type
  String *typeFromTypemap;
  if ((typeFromTypemap = Swig_typemap_lookup("typescripttype", n, "", 0)))
  {
    Printf(stdout, "##### 10 - Type is returned from typemap : %s \n", typeFromTypemap);
    return typeFromTypemap;
  }

  SwigType *type = Getattr(n, "type");
  Printf(stdout, "##### 19 - Type to be checked : %s \n", type);
  String *typescriptType = getProxyName(type);
  Printf(stdout, "##### 19 - Proxy Type : %s \n", typescriptType);
  // String *substType = NewString("$classname");
  // substituteClassnameSpecialVariable(type, substType, "$javaclassname");
  // Printf(stdout, "##### 19 - SubstType Type : %s \n", substType);
  return typescriptType;
}

/* -----------------------------------------------------------------------------
   * substituteClassnameSpecialVariable()
   * ----------------------------------------------------------------------------- */

void JAVASCRIPT::substituteClassnameSpecialVariable(SwigType *classnametype, String *tm, const char *classnamespecialvariable, bool jnidescriptor)
{
  String *replacementname;

  if (SwigType_isenum(classnametype))
  {
    String *enumname = getEnumName(classnametype, jnidescriptor);
    if (enumname)
    {
      replacementname = Copy(enumname);
    }
    else
    {
      bool anonymous_enum = (Cmp(classnametype, "enum ") == 0);
      if (anonymous_enum)
      {
        replacementname = NewString("int");
      }
      else
      {
        // An unknown enum - one that has not been parsed (neither a C enum forward reference nor a definition) or an ignored enum
        replacementname = NewStringf("SWIGTYPE%s", SwigType_manglestr(classnametype));
        Replace(replacementname, "enum ", "", DOH_REPLACE_ANY);
        // Setattr(swig_types_hash, replacementname, classnametype);
      }
    }
  }
  else
  {
    String *classname = getProxyName(classnametype, jnidescriptor); // getProxyName() works for pointers to classes too
    if (classname)
    {
      replacementname = Copy(classname);
    }
    else
    {
      // use $descriptor if SWIG does not know anything about this type. Note that any typedefs are resolved.
      replacementname = NewStringf("SWIGTYPE%s", SwigType_manglestr(classnametype));

      // Add to hash table so that the type wrapper classes can be created later
      // Setattr(swig_types_hash, replacementname, classnametype);
    }
  }
  if (jnidescriptor)
    Replaceall(replacementname, ".", "/");
  Printf(stdout, "##### 10 Class name replacement variable : %s\n", replacementname);
  Replaceall(tm, classnamespecialvariable, replacementname);

  Delete(replacementname);
}

/* -----------------------------------------------------------------------------
   * getEnumName()
   *
   * If jnidescriptor is set, inner class names are separated with '$' otherwise a '.'
   * and the package is also not added to the name.
   * ----------------------------------------------------------------------------- */

String *JAVASCRIPT::getEnumName(SwigType *t, bool jnidescriptor)
{
  Node *enumname = NULL;
  Node *n = enumLookup(t);
  if (n)
  {
    enumname = Getattr(n, "enumname");
    if (!enumname || jnidescriptor)
    {
      String *symname = Getattr(n, "sym:name");
      if (symname)
      {
        // Add in class scope when referencing enum if not a global enum
        String *scopename_prefix = Swig_scopename_prefix(Getattr(n, "name"));
        String *proxyname = 0;
        if (scopename_prefix)
        {
          proxyname = getProxyName(scopename_prefix, jnidescriptor);
        }
        if (proxyname)
        {
          const char *class_separator = jnidescriptor ? "$" : ".";
          enumname = NewStringf("%s%s%s", proxyname, class_separator, symname);
        }
        else
        {
          // global enum or enum in a namespace
          String *nspace = Getattr(n, "sym:nspace");
          if (nspace)
          {
            // if (package && !jnidescriptor)
            //   enumname = NewStringf("%s.%s.%s", package, nspace, symname);
            // else
            enumname = NewStringf("%s.%s", nspace, symname);
          }
          else
          {
            enumname = Copy(symname);
          }
        }
        if (!jnidescriptor)
        {
          Setattr(n, "enumname", enumname); // Cache it
          Delete(enumname);
        }
        Delete(scopename_prefix);
      }
    }
  }

  return enumname;
}

int JAVASCRIPT::fragmentDirective(Node *n)
{

  // catch all fragment directives that have "templates" as location
  // and register them at the emitter.
  String *section = Getattr(n, "section");

  if (Equal(section, "templates") && !ImportMode)
  {
    emitter->registerTemplate(Getattr(n, "value"), Getattr(n, "code"));
  }
  else
  {
    return Language::fragmentDirective(n);
  }

  return SWIG_OK;
}

String *JAVASCRIPT::getNSpace() const
{
  return Language::getNSpace();
}

/* ---------------------------------------------------------------------
 * top()
 *
 * Function handler for processing top node of the parse tree
 * Wrapper code generation essentially starts from here
 * --------------------------------------------------------------------- */

int JAVASCRIPT::top(Node *n)
{
  emitter->initialize(n);
  imclass_cppcasts_code = NewString("");
  Printf(stdout, "Top Node : %s \n", Getattr(n, "name"));
  generateBaseInterfaces(n);

  Language::top(n);

  emitter->dump(n);
  emitter->close();

  return SWIG_OK;
}

static const char *usage = (char *)"\
Javascript Options (available with -javascript)\n\
     -jsc                   - creates a JavascriptCore extension \n\
     -v8                    - creates a v8 extension \n\
     -node                  - creates a node.js extension \n\
     -debug-codetemplates   - generates information about the origin of code templates\n";

/* ---------------------------------------------------------------------
 * main()
 *
 * Entry point for the JAVASCRIPT module
 * --------------------------------------------------------------------- */

void JAVASCRIPT::main(int argc, char *argv[])
{
  // Set javascript subdirectory in SWIG library
  SWIG_library_directory("javascript");

  int engine = -1;

  for (int i = 1; i < argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(argv[i], "-v8") == 0)
      {
        if (engine != -1)
        {
          Printf(stderr, ERR_MSG_ONLY_ONE_ENGINE_PLEASE);
          SWIG_exit(-1);
        }
        Swig_mark_arg(i);
        engine = JSEmitter::V8;
      }
      else if (strcmp(argv[i], "-jsc") == 0)
      {
        if (engine != -1)
        {
          Printf(stderr, ERR_MSG_ONLY_ONE_ENGINE_PLEASE);
          SWIG_exit(-1);
        }
        Swig_mark_arg(i);
        engine = JSEmitter::JavascriptCore;
      }
      else if (strcmp(argv[i], "-node") == 0)
      {
        if (engine != -1)
        {
          Printf(stderr, ERR_MSG_ONLY_ONE_ENGINE_PLEASE);
          SWIG_exit(-1);
        }
        Swig_mark_arg(i);
        engine = JSEmitter::NodeJS;
      }
      else if (strcmp(argv[i], "-debug-codetemplates") == 0)
      {
        Swig_mark_arg(i);
        js_template_enable_debug = true;
      }
      else if (strcmp(argv[i], "-help") == 0)
      {
        fputs(usage, stdout);
        return;
      }
    }

    Swig_interface_feature_enable();
  }

  switch (engine)
  {
  case JSEmitter::V8:
  {
    emitter = swig_javascript_create_V8Emitter();
    Preprocessor_define("SWIG_JAVASCRIPT_V8 1", 0);
    SWIG_library_directory("javascript/v8");
    // V8 API is C++, so output must be C++ compatible even when wrapping C code
    if (!cparse_cplusplus)
    {
      Swig_cparse_cplusplusout(1);
    }
    break;
  }
  case JSEmitter::JavascriptCore:
  {
    emitter = swig_javascript_create_JSCEmitter();
    Preprocessor_define("SWIG_JAVASCRIPT_JSC 1", 0);
    SWIG_library_directory("javascript/jsc");
    break;
  }
  case JSEmitter::NodeJS:
  {
    Printf(stdout, "JS Emitter NodeJS\n");
    emitter = swig_javascript_create_V8Emitter();
    Preprocessor_define("SWIG_JAVASCRIPT_V8 1", 0);
    Preprocessor_define("BUILDING_NODE_EXTENSION 1", 0);
    SWIG_library_directory("javascript/v8");
    break;
  }
  default:
  {
    Printf(stderr, "SWIG Javascript: Unknown engine. Please specify one of '-jsc', '-v8' or '-node'.\n");
    SWIG_exit(-1);
    break;
  }
  }

  // Add a symbol to the parser for conditional compilation
  Preprocessor_define("SWIGJAVASCRIPT 1", 0);

  // Add typemap definitions
  SWIG_typemap_lang("javascript");

  // Set configuration file
  SWIG_config_file("javascript.swg");

  allow_overloading();
}

/* -----------------------------------------------------------------------------
 * swig_javascript()    - Instantiate module
 * ----------------------------------------------------------------------------- */

static Language *new_swig_javascript()
{
  return new JAVASCRIPT();
}

extern "C" Language *swig_javascript(void)
{
  return new_swig_javascript();
}

/**********************************************************************
 * Emitter implementations
 **********************************************************************/

/* -----------------------------------------------------------------------------
 * JSEmitter()
 * ----------------------------------------------------------------------------- */

JSEmitter::JSEmitter(JSEmitter::JSEngine engine)
    : engine(engine), templates(NewHash()), namespaces(NULL), current_namespace(NULL), defaultResultName(NewString("result")), f_wrappers(NULL)
{
}

/* -----------------------------------------------------------------------------
 * ~JSEmitter()
 * ----------------------------------------------------------------------------- */

JSEmitter::~JSEmitter()
{
  Delete(templates);
}

/* -----------------------------------------------------------------------------
 * JSEmitter::RegisterTemplate() :  Registers a code template
 *
 *  Note: this is used only by JAVASCRIPT::fragmentDirective().
 * ----------------------------------------------------------------------------- */

int JSEmitter::registerTemplate(const String *name, const String *code)
{
  if (!State::IsSet(state.globals(HAS_TEMPLATES)))
  {
    SetFlag(state.globals(), HAS_TEMPLATES);
  }
  return Setattr(templates, name, code);
}

/* -----------------------------------------------------------------------------
 * JSEmitter::getTemplate() :  Provides a registered code template
 * ----------------------------------------------------------------------------- */

Template JSEmitter::getTemplate(const String *name)
{
  String *templ = Getattr(templates, name);

  if (!templ)
  {
    Printf(stderr, "Could not find template %s\n.", name);
    SWIG_exit(EXIT_FAILURE);
  }

  Template t(templ, name);
  return t;
}

JSEmitterState &JSEmitter::getState()
{
  return state;
}

int JSEmitter::initialize(Node * /*n */)
{

  if (namespaces != NULL)
  {
    Delete(namespaces);
  }
  namespaces = NewHash();
  Hash *global_namespace = createNamespaceEntry("exports", 0, 0);

  Setattr(namespaces, "::", global_namespace);
  current_namespace = global_namespace;

  f_wrappers = NewString("");

  return SWIG_OK;
}

/* ---------------------------------------------------------------------
 * skipIgnoredArgs()
 * --------------------------------------------------------------------- */

Parm *JSEmitter::skipIgnoredArgs(Parm *p)
{
  while (checkAttribute(p, "tmap:in:numinputs", "0"))
  {
    p = Getattr(p, "tmap:in:next");
  }
  return p;
}

/* -----------------------------------------------------------------------------
 * JSEmitter::getBaseClass() :  the node of the base class or NULL
 *
 * Note: the first base class is provided. Multiple inheritance is not
 *       supported.
 * ----------------------------------------------------------------------------- */

Node *JSEmitter::getBaseClass(Node *n)
{
  // retrieve the first base class that is not %ignored
  List *baselist = Getattr(n, "bases");
  if (baselist)
  {
    Iterator base = First(baselist);
    while (base.item && GetFlag(base.item, "feature:ignore"))
    {
      base = Next(base);
    }
    return base.item;
  }
  return NULL;
}

/* -----------------------------------------------------------------------------
  * JSEmitter::emitWrapperFunction() :  dispatches emitter functions.
  *
  * This allows to have small sized, dedicated emitting functions.
  * All state dependent branching is done here.
  * ----------------------------------------------------------------------------- */

int JSEmitter::emitWrapperFunction(Node *n)
{
  int ret = SWIG_OK;

  String *kind = Getattr(n, "kind");

  if (kind)
  {

    if (Equal(kind, "function")
        // HACK: sneaky.ctest revealed that typedef'd (global) functions must be
        // detected via the 'view' attribute.
        || (Equal(kind, "variable") && Equal(Getattr(n, "view"), "globalfunctionHandler")))
    {
      bool is_member = GetFlag(n, "ismember") != 0 || GetFlag(n, "feature:extend") != 0;
      bool is_static = GetFlag(state.function(), IS_STATIC) != 0;
      ret = emitFunction(n, is_member, is_static);
    }
    else if (Cmp(kind, "variable") == 0)
    {
      bool is_static = GetFlag(state.variable(), IS_STATIC) != 0;
      // HACK: smartpointeraccessed static variables are not treated as statics
      if (GetFlag(n, "allocate:smartpointeraccess"))
      {
        is_static = false;
      }

      bool is_member = GetFlag(n, "ismember") != 0;
      bool is_setter = GetFlag(n, "memberset") != 0 || GetFlag(n, "varset") != 0;
      bool is_getter = GetFlag(n, "memberget") != 0 || GetFlag(n, "varget") != 0;
      if (is_setter)
      {
        ret = emitSetter(n, is_member, is_static);
      }
      else if (is_getter)
      {
        ret = emitGetter(n, is_member, is_static);
      }
      else
      {
        Swig_print_node(n);
      }
    }
    else
    {
      Printf(stderr, "Warning: unsupported wrapper function type\n");
      Swig_print_node(n);
      ret = SWIG_ERROR;
    }
  }
  else
  {
    String *view = Getattr(n, "view");

    if (Cmp(view, "constructorHandler") == 0)
    {
      ret = emitCtor(n);
    }
    else if (Cmp(view, "destructorHandler") == 0)
    {
      ret = emitDtor(n);
    }
    else
    {
      Printf(stderr, "Warning: unsupported wrapper function type");
      Swig_print_node(n);
      ret = SWIG_ERROR;
    }
  }

  return ret;
}

int JSEmitter::emitNativeFunction(Node *n)
{
  String *wrapname = Getattr(n, "wrap:name");
  enterFunction(n);
  state.function(WRAPPER_NAME, wrapname);
  exitFunction(n);
  return SWIG_OK;
}

int JSEmitter::enterClass(Node *n)
{
  state.clazz(RESET);
  state.clazz(NAME, Getattr(n, "sym:name"));
  state.clazz("nspace", current_namespace);

  // Creating a mangled name using the current namespace and the symbol name
  String *mangled_name = NewString("");
  Printf(mangled_name, "%s_%s", Getattr(current_namespace, NAME_MANGLED), Getattr(n, "sym:name"));
  state.clazz(NAME_MANGLED, SwigType_manglestr(mangled_name));
  Delete(mangled_name);

  state.clazz(TYPE, NewString(Getattr(n, "classtype")));

  String *type = SwigType_manglestr(Getattr(n, "classtypeobj"));
  String *classtype_mangled = NewString("");
  Printf(classtype_mangled, "p%s", type);
  state.clazz(TYPE_MANGLED, classtype_mangled);
  Delete(type);

  String *ctor_wrapper = NewString("_wrap_new_veto_");
  Append(ctor_wrapper, state.clazz(NAME));
  state.clazz(CTOR, ctor_wrapper);
  state.clazz(CTOR_DISPATCHERS, NewString(""));
  state.clazz(DTOR, NewString("0"));

  // HACK: assume that a class is abstract
  // this is resolved by emitCtor (which is only called for non abstract classes)
  SetFlag(state.clazz(), IS_ABSTRACT);

  return SWIG_OK;
}

int JSEmitter::enterFunction(Node *n)
{
  state.function(RESET);
  state.function(NAME, Getattr(n, "sym:name"));
  if (Equal(Getattr(n, "storage"), "static"))
  {
    SetFlag(state.function(), IS_STATIC);
  }
  return SWIG_OK;
}

int JSEmitter::enterVariable(Node *n)
{
  // reset the state information for variables.
  state.variable(RESET);

  // Retrieve a pure symbol name. Using 'sym:name' as a basis, as it considers %renamings.
  if (Equal(Getattr(n, "view"), "memberconstantHandler"))
  {
    // Note: this is kind of hacky/experimental
    // For constants/enums 'sym:name' contains e.g., 'Foo_Hello' instead of 'Hello'
    state.variable(NAME, Getattr(n, "memberconstantHandler:sym:name"));
  }
  else
  {
    state.variable(NAME, Swig_scopename_last(Getattr(n, "sym:name")));
  }

  if (Equal(Getattr(n, "storage"), "static"))
  {
    SetFlag(state.variable(), IS_STATIC);
  }

  if (!Language::instance()->is_assignable(n))
  {
    SetFlag(state.variable(), IS_IMMUTABLE);
  }
  // FIXME: test "arrays_global" does not compile with that as it is not allowed to assign to char[]
  if (Equal(Getattr(n, "type"), "a().char"))
  {
    SetFlag(state.variable(), IS_IMMUTABLE);
  }

  return SWIG_OK;
}

int JSEmitter::emitCtor(Node *n)
{

  Wrapper *wrapper = NewWrapper();

  bool is_overloaded = GetFlag(n, "sym:overloaded") != 0;

  Template t_ctor(getTemplate("js_ctor"));

  String *wrap_name = Swig_name_wrapper(Getattr(n, "sym:name"));
  if (is_overloaded)
  {
    t_ctor = getTemplate("js_overloaded_ctor");
    Append(wrap_name, Getattr(n, "sym:overname"));
  }
  Setattr(n, "wrap:name", wrap_name);
  // note: we can remove the is_abstract flag now, as this
  //       is called for non-abstract classes only.
  Setattr(state.clazz(), IS_ABSTRACT, 0);

  ParmList *params = Getattr(n, "parms");
  emit_parameter_variables(params, wrapper);
  emit_attach_parmmaps(params, wrapper);
  // HACK: in test-case `ignore_parameter` emit_attach_parmmaps generated an extra line of applied typemaps.
  // Deleting wrapper->code here, to reset, and as it seemed to have no side effect elsewhere
  Delete(wrapper->code);
  wrapper->code = NewString("");

  Printf(wrapper->locals, "%sresult;", SwigType_str(Getattr(n, "type"), 0));

  marshalInputArgs(n, params, wrapper, Ctor, true, false);
  String *action = emit_action(n);
  Printv(wrapper->code, action, "\n", 0);

  emitCleanupCode(n, wrapper, params);

  t_ctor.replace("$jswrapper", wrap_name)
      .replace("$jsmangledtype", state.clazz(TYPE_MANGLED))
      .replace("$jslocals", wrapper->locals)
      .replace("$jscode", wrapper->code)
      .replace("$jsargcount", Getattr(n, ARGCOUNT))
      .pretty_print(f_wrappers);

  Template t_ctor_case(getTemplate("js_ctor_dispatch_case"));
  t_ctor_case.replace("$jswrapper", wrap_name)
      .replace("$jsargcount", Getattr(n, ARGCOUNT));
  Append(state.clazz(CTOR_DISPATCHERS), t_ctor_case.str());

  DelWrapper(wrapper);

  // create a dispatching ctor
  if (is_overloaded)
  {
    if (!Getattr(n, "sym:nextSibling"))
    {
      String *wrap_name = Swig_name_wrapper(Getattr(n, "sym:name"));
      Template t_mainctor(getTemplate("js_ctor_dispatcher"));
      t_mainctor.replace("$jswrapper", wrap_name)
          .replace("$jsmangledname", state.clazz(NAME_MANGLED))
          .replace("$jsdispatchcases", state.clazz(CTOR_DISPATCHERS))
          .pretty_print(f_wrappers);
      state.clazz(CTOR, wrap_name);
    }
  }
  else
  {
    state.clazz(CTOR, wrap_name);
  }

  return SWIG_OK;
}

int JSEmitter::emitDtor(Node *n)
{

  String *wrap_name = Swig_name_wrapper(Getattr(n, "sym:name"));

  SwigType *type = state.clazz(TYPE);
  String *p_classtype = SwigType_add_pointer(state.clazz(TYPE));
  String *ctype = SwigType_lstr(p_classtype, "");
  String *free = NewString("");

  // (Taken from JSCore implementation.)
  /* The if (Extend) block was taken from the Ruby implementation.
   * The problem is that in the case of an %extend to create a destructor for a struct to coordinate automatic memory cleanup with the Javascript collector,
   * the SWIG function was not being generated. More specifically:
   struct MyData {
   %extend {
   ~MyData() {
   FreeData($self);
   }
   }
   };
   %newobject CreateData;
   struct MyData* CreateData(void);
   %delobject FreeData;
   void FreeData(struct MyData* the_data);

   where the use case is something like:
   var my_data = example.CreateData();
   my_data = null;

   This function was not being generated:
   SWIGINTERN void delete_MyData(struct MyData *self){
   FreeData(self);
   }

   I don't understand fully why it wasn't being generated. It just seems to happen in the Lua generator.
   There is a comment about staticmemberfunctionHandler having an inconsistency and I tracked down dome of the SWIGINTERN void delete_*
   code to that function in the Language base class.
   The Ruby implementation seems to have an explicit check for if(Extend) and explicitly generates the code, so that's what I'm doing here.
   The Ruby implementation does other stuff which I omit.
   */
  if (Extend)
  {
    String *wrap = Getattr(n, "wrap:code");
    if (wrap)
    {
      Printv(f_wrappers, wrap, NIL);
    }
  }
  // HACK: this is only for the v8 emitter. maybe set an attribute wrap:action of node
  // TODO: generate dtors more similar to other wrappers
  // EW: I think this is wrong. delete should only be used when new was used to create. If malloc was used, free needs to be used.
  if (SwigType_isarray(type))
  {
    Printf(free, "delete [] (%s)", ctype);
  }
  else
  {
    Printf(free, "delete (%s)", ctype);
  }

  String *destructor_action = Getattr(n, "wrap:action");
  // Adapted from the JSCore implementation.
  /* The next challenge is to generate the correct finalize function for JavaScriptCore to call.
     Originally, it would use this fragment from javascriptcode.swg
     %fragment ("JS_destructordefn", "templates")
     %{
     void _wrap_${classname_mangled}_finalize(JSObjectRef thisObject)
     {
     SWIG_PRV_DATA* t = (SWIG_PRV_DATA*)JSObjectGetPrivate(thisObject);
     if(t && t->swigCMemOwn) free ((${type}*)t->swigCObject);
     if(t) free(t);
     }
     %}

     But for the above example case of %extend to define a destructor on a struct, we need to override the system to not call
     free ((${type}*)t->swigCObject);
     and substitute it with what the user has provided.
     To solve this, I created a variation fragment called JS_destructoroverridedefn:
     SWIG_PRV_DATA* t = (SWIG_PRV_DATA*)JSObjectGetPrivate(thisObject);
     if(t && t->swigCMemOwn) {
     ${type}* arg1 = (${type}*)t->swigCObject;
     ${destructor_action}
     }
     if(t) free(t);

     Based on what I saw in the Lua and Ruby modules, I use Getattr(n, "wrap:action")
     to decide if the user has a preferred destructor action.
     Based on that, I decide which fragment to use.
     And in the case of the custom action, I substitute that action in.
     I noticed that destructor_action has the form
     delete_MyData(arg1);
     The explicit arg1 is a little funny, so I structured the fragment to create a temporary variable called arg1 to make the generation easier.
     This might suggest this solution misunderstands a more complex case.

     Also, there is a problem where destructor_action is always true for me, even when not requesting %extend as above.
     So this code doesn't actually quite work as I expect. The end result is that the code still works because
     destructor_action calls free like the original template. The one caveat is the string in destructor_action casts to char* which is weird.
     I think there is a deeper underlying SWIG issue because I don't think it should be char*. However, it doesn't really matter for free.

     Maybe the fix for the destructor_action always true problem is that this is supposed to be embedded in the if(Extend) block above.
     But I don't fully understand the conditions of any of these things, and since it works for the moment, I don't want to break more stuff.
   */
  if (destructor_action)
  {
    Template t_dtor = getTemplate("js_dtoroverride");
    state.clazz(DTOR, wrap_name);
    t_dtor.replace("${classname_mangled}", state.clazz(NAME_MANGLED))
        .replace("$jswrapper", wrap_name)
        .replace("$jsfree", free)
        .replace("$jstype", ctype);

    t_dtor.replace("${destructor_action}", destructor_action);
    Wrapper_pretty_print(t_dtor.str(), f_wrappers);
  }
  else
  {
    Template t_dtor = getTemplate("js_dtor");
    state.clazz(DTOR, wrap_name);
    t_dtor.replace("$jsmangledname", state.clazz(NAME_MANGLED))
        .replace("$jswrapper", wrap_name)
        .replace("$jsfree", free)
        .replace("$jstype", ctype)
        .pretty_print(f_wrappers);
  }

  Delete(p_classtype);
  Delete(ctype);
  Delete(free);

  return SWIG_OK;
}

int JSEmitter::emitGetter(Node *n, bool is_member, bool is_static)
{
  Wrapper *wrapper = NewWrapper();
  Template t_getter(getTemplate("js_getter"));

  // prepare wrapper name
  String *wrap_name = Swig_name_wrapper(Getattr(n, "sym:name"));
  Setattr(n, "wrap:name", wrap_name);
  state.variable(GETTER, wrap_name);

  // prepare local variables
  ParmList *params = Getattr(n, "parms");
  emit_parameter_variables(params, wrapper);
  emit_attach_parmmaps(params, wrapper);

  // prepare code part
  String *action = emit_action(n);
  marshalInputArgs(n, params, wrapper, Getter, is_member, is_static);
  marshalOutput(n, params, wrapper, action);

  emitCleanupCode(n, wrapper, params);

  t_getter.replace("$jswrapper", wrap_name)
      .replace("$jslocals", wrapper->locals)
      .replace("$jscode", wrapper->code)
      .pretty_print(f_wrappers);

  DelWrapper(wrapper);

  return SWIG_OK;
}

int JSEmitter::emitSetter(Node *n, bool is_member, bool is_static)
{

  // skip variables that are immutable
  if (State::IsSet(state.variable(IS_IMMUTABLE)))
  {
    return SWIG_OK;
  }

  Wrapper *wrapper = NewWrapper();

  Template t_setter(getTemplate("js_setter"));

  // prepare wrapper name
  String *wrap_name = Swig_name_wrapper(Getattr(n, "sym:name"));
  Setattr(n, "wrap:name", wrap_name);
  state.variable(SETTER, wrap_name);

  // prepare local variables
  ParmList *params = Getattr(n, "parms");
  emit_parameter_variables(params, wrapper);
  emit_attach_parmmaps(params, wrapper);

  // prepare code part
  String *action = emit_action(n);
  marshalInputArgs(n, params, wrapper, Setter, is_member, is_static);
  Append(wrapper->code, action);

  emitCleanupCode(n, wrapper, params);

  t_setter.replace("$jswrapper", wrap_name)
      .replace("$jslocals", wrapper->locals)
      .replace("$jscode", wrapper->code)
      .pretty_print(f_wrappers);

  DelWrapper(wrapper);

  return SWIG_OK;
}

/* -----------------------------------------------------------------------------
 * JSEmitter::emitConstant() :  triggers code generation for constants
 * ----------------------------------------------------------------------------- */

int JSEmitter::emitConstant(Node *n)
{
  // HACK: somehow it happened under Mac OS X that before everything started
  // a lot of SWIG internal constants were emitted
  // This didn't happen on other platforms yet...
  // we ignore those premature definitions
  if (!State::IsSet(state.globals(HAS_TEMPLATES)))
  {
    return SWIG_ERROR;
  }

  Wrapper *wrapper = NewWrapper();
  SwigType *type = Getattr(n, "type");
  String *name = Getattr(n, "name");
  String *iname = Getattr(n, "sym:name");
  String *wname = Swig_name_wrapper(name);
  String *rawval = Getattr(n, "rawval");
  String *value = rawval ? rawval : Getattr(n, "value");

  // HACK: forcing usage of cppvalue for v8 (which turned out to fix typdef_struct.i, et. al)
  if (State::IsSet(state.globals(FORCE_CPP)) && Getattr(n, "cppvalue") != NULL)
  {
    value = Getattr(n, "cppvalue");
  }

  Template t_getter(getTemplate("js_getter"));

  // call the variable methods as a constants are
  // registered in same way
  enterVariable(n);
  state.variable(GETTER, wname);
  // TODO: why do we need this?
  Setattr(n, "wrap:name", wname);

  // special treatment of member pointers
  if (SwigType_type(type) == T_MPOINTER)
  {
    // TODO: this could go into a code-template
    String *mpointer_wname = NewString("");
    Printf(mpointer_wname, "_wrapConstant_%s", iname);
    Setattr(n, "memberpointer:constant:wrap:name", mpointer_wname);
    String *str = SwigType_str(type, mpointer_wname);
    Printf(f_wrappers, "static %s = %s;\n", str, value);
    Delete(str);
    value = mpointer_wname;
  }

  marshalOutput(n, 0, wrapper, NewString(""), value, false);

  t_getter.replace("$jswrapper", wname)
      .replace("$jslocals", wrapper->locals)
      .replace("$jscode", wrapper->code)
      .pretty_print(f_wrappers);

  exitVariable(n);

  DelWrapper(wrapper);

  return SWIG_OK;
}

int JSEmitter::emitFunction(Node *n, bool is_member, bool is_static)
{
  Wrapper *wrapper = NewWrapper();
  Template t_function(getTemplate("js_function"));

  bool is_overloaded = GetFlag(n, "sym:overloaded") != 0;

  // prepare the function wrapper name
  String *iname = Getattr(n, "sym:name");
  String *wrap_name = Swig_name_wrapper(iname);
  if (is_overloaded)
  {
    t_function = getTemplate("js_overloaded_function");
    Append(wrap_name, Getattr(n, "sym:overname"));
  }
  Setattr(n, "wrap:name", wrap_name);
  state.function(WRAPPER_NAME, wrap_name);

  // prepare local variables
  ParmList *params = Getattr(n, "parms");
  emit_parameter_variables(params, wrapper);
  emit_attach_parmmaps(params, wrapper);

  // HACK: in test-case `ignore_parameter` emit_attach_parmmaps generates an extra line of applied typemap.
  // Deleting wrapper->code here fixes the problem, and seems to have no side effect elsewhere
  Delete(wrapper->code);
  wrapper->code = NewString("");

  marshalInputArgs(n, params, wrapper, Function, is_member, is_static);
  String *action = emit_action(n);
  marshalOutput(n, params, wrapper, action);
  emitCleanupCode(n, wrapper, params);
  Replaceall(wrapper->code, "$symname", iname);

  t_function.replace("$jswrapper", wrap_name)
      .replace("$jslocals", wrapper->locals)
      .replace("$jscode", wrapper->code)
      .replace("$jsargcount", Getattr(n, ARGCOUNT))
      .pretty_print(f_wrappers);

  DelWrapper(wrapper);

  return SWIG_OK;
}

int JSEmitter::emitFunctionDispatcher(Node *n, bool /*is_member */)
{
  Wrapper *wrapper = NewWrapper();

  // Generate call list, go to first node
  Node *sibl = n;

  while (Getattr(sibl, "sym:previousSibling"))
    sibl = Getattr(sibl, "sym:previousSibling"); // go all the way up

  do
  {
    String *siblname = Getattr(sibl, "wrap:name");

    if (siblname)
    {
      // handle function overloading
      Template t_dispatch_case = getTemplate("js_function_dispatch_case");
      t_dispatch_case.replace("$jswrapper", siblname)
          .replace("$jsargcount", Getattr(sibl, ARGCOUNT));

      Append(wrapper->code, t_dispatch_case.str());
    }

  } while ((sibl = Getattr(sibl, "sym:nextSibling")));

  Template t_function(getTemplate("js_function_dispatcher"));

  // Note: this dispatcher function gets called after the last overloaded function has been created.
  // At this time, n.wrap:name contains the name of the last wrapper function.
  // To get a valid function name for the dispatcher function we take the last wrapper name and
  // substract the extension "sym:overname",
  String *wrap_name = NewString(Getattr(n, "wrap:name"));
  String *overname = Getattr(n, "sym:overname");

  Node *methodclass = Swig_methodclass(n);
  String *class_name = Getattr(methodclass, "sym:name");

  int l1 = Len(wrap_name);
  int l2 = Len(overname);
  Delslice(wrap_name, l1 - l2, l1);

  String *new_string = NewStringf("%s_%s", class_name, wrap_name);
  String *final_wrap_name = Swig_name_wrapper(new_string);

  Setattr(n, "wrap:name", final_wrap_name);
  state.function(WRAPPER_NAME, final_wrap_name);

  t_function.replace("$jslocals", wrapper->locals)
      .replace("$jscode", wrapper->code);

  // call this here, to replace all variables
  t_function.replace("$jswrapper", final_wrap_name)
      .replace("$jsname", state.function(NAME))
      .pretty_print(f_wrappers);

  // Delete the state variable
  DelWrapper(wrapper);

  return SWIG_OK;
}

String *JSEmitter::emitInputTypemap(Node *n, Parm *p, Wrapper *wrapper, String *arg)
{
  // Get input typemap for current param
  String *tm = Getattr(p, "tmap:in");
  SwigType *type = Getattr(p, "type");

  if (tm != NULL)
  {
    Replaceall(tm, "$input", arg);
    Setattr(p, "emit:input", arg);
    // do replacements for built-in variables
    if (Getattr(p, "wrap:disown") || (Getattr(p, "tmap:in:disown")))
    {
      Replaceall(tm, "$disown", "SWIG_POINTER_DISOWN");
    }
    else
    {
      Replaceall(tm, "$disown", "0");
    }
    Replaceall(tm, "$symname", Getattr(n, "sym:name"));
    Printf(wrapper->code, "%s\n", tm);
  }
  else
  {
    Swig_warning(WARN_TYPEMAP_IN_UNDEF, input_file, line_number, "Unable to use type %s as a function argument.\n", SwigType_str(type, 0));
  }

  return tm;
}

void JSEmitter::marshalOutput(Node *n, ParmList *params, Wrapper *wrapper, String *actioncode, const String *cresult, bool emitReturnVariable)
{
  SwigType *type = Getattr(n, "type");
  String *tm;
  Parm *p;

  // adds a declaration for the result variable
  if (emitReturnVariable)
    emit_return_variable(n, type, wrapper);
  // if not given, use default result identifier ('result') for output typemap
  if (cresult == 0)
    cresult = defaultResultName;

  tm = Swig_typemap_lookup_out("out", n, cresult, wrapper, actioncode);
  bool should_own = GetFlag(n, "feature:new") != 0;

  if (tm)
  {
    Replaceall(tm, "$objecttype", Swig_scopename_last(SwigType_str(SwigType_strip_qualifiers(type), 0)));

    if (should_own)
    {
      Replaceall(tm, "$owner", "SWIG_POINTER_OWN");
    }
    else
    {
      Replaceall(tm, "$owner", "0");
    }
    Append(wrapper->code, tm);

    if (Len(tm) > 0)
    {
      Printf(wrapper->code, "\n");
    }
  }
  else
  {
    Swig_warning(WARN_TYPEMAP_OUT_UNDEF, input_file, line_number, "Unable to use return type %s in function %s.\n", SwigType_str(type, 0), Getattr(n, "name"));
  }

  if (params)
  {
    for (p = params; p;)
    {
      if ((tm = Getattr(p, "tmap:argout")))
      {
        Replaceall(tm, "$input", Getattr(p, "emit:input"));
        Printv(wrapper->code, tm, "\n", NIL);
        p = Getattr(p, "tmap:argout:next");
      }
      else
      {
        p = nextSibling(p);
      }
    }
  }

  Replaceall(wrapper->code, "$result", "jsresult");
}

void JSEmitter::emitCleanupCode(Node *n, Wrapper *wrapper, ParmList *params)
{
  Parm *p;
  String *tm;

  for (p = params; p;)
  {
    if ((tm = Getattr(p, "tmap:freearg")))
    {
      //addThrows(n, "tmap:freearg", p);
      Replaceall(tm, "$input", Getattr(p, "emit:input"));
      Printv(wrapper->code, tm, "\n", NIL);
      p = Getattr(p, "tmap:freearg:next");
    }
    else
    {
      p = nextSibling(p);
    }
  }

  if (GetFlag(n, "feature:new"))
  {
    tm = Swig_typemap_lookup("newfree", n, Swig_cresult_name(), 0);
    if (tm != NIL)
    {
      //addThrows(throws_hash, "newfree", n);
      Printv(wrapper->code, tm, "\n", NIL);
    }
  }

  /* See if there is any return cleanup code */
  if ((tm = Swig_typemap_lookup("ret", n, Swig_cresult_name(), 0)))
  {
    Printf(wrapper->code, "%s\n", tm);
    Delete(tm);
  }
}

int JSEmitter::switchNamespace(Node *n)
{
  // HACK: somehow this gets called for member functions.
  // We can safely ignore them, as members are not associated to a namespace (only their class)
  if (GetFlag(n, "ismember"))
  {
    return SWIG_OK;
  }

  // if nspace is deactivated, everything goes into the global scope
  if (!GetFlag(n, "feature:nspace"))
  {
    current_namespace = Getattr(namespaces, "::");
    return SWIG_OK;
  }

// EXPERIMENTAL: we want to use Language::getNSpace() here
// However, it is not working yet.
// For namespace functions Language::getNSpace() does not give a valid result
#if 0
  JAVASCRIPT *lang = static_cast<JAVASCRIPT*>(Language::instance());
  String *_nspace = lang->getNSpace();
  if (!Equal(nspace, _nspace)) {
    Printf(stdout, "##### Custom vs Language::getNSpace(): %s | %s\n", nspace, _nspace);
    Swig_print_node(n);
  }
#endif

  String *nspace = Getattr(n, "sym:nspace");

  if (nspace == NULL)
  {
    // It seems that only classes have 'sym:nspace' set.
    // We try to get the namespace from the qualified name (i.e., everything before the last '::')
    nspace = Swig_scopename_prefix(Getattr(n, "name"));
  }

  // If there is not even a scopename prefix then it must be global scope
  if (nspace == NULL)
  {
    current_namespace = Getattr(namespaces, "::");
    return SWIG_OK;
  }

  String *scope = NewString(nspace);
  // replace "." with "::" that we can use Swig_scopename_last
  Replaceall(scope, ".", "::");

  // if the scope is not yet registered
  // create (parent) namespaces recursively
  if (!Getattr(namespaces, scope))
  {
    createNamespace(scope);
  }
  current_namespace = Getattr(namespaces, scope);

  return SWIG_OK;
}

int JSEmitter::createNamespace(String *scope)
{

  String *parent_scope = Swig_scopename_prefix(scope);
  Hash *parent_namespace;
  if (parent_scope == 0)
  {
    parent_namespace = Getattr(namespaces, "::");
  }
  else if (!Getattr(namespaces, parent_scope))
  {
    createNamespace(parent_scope);
    parent_namespace = Getattr(namespaces, parent_scope);
  }
  else
  {
    parent_namespace = Getattr(namespaces, parent_scope);
  }
  assert(parent_namespace != 0);

  Hash *new_namespace = createNamespaceEntry(Char(scope), Char(Getattr(parent_namespace, "name")), Char(Getattr(parent_namespace, "name_mangled")));
  Setattr(namespaces, scope, new_namespace);

  Delete(parent_scope);
  return SWIG_OK;
}

Hash *JSEmitter::createNamespaceEntry(const char *_name, const char *parent, const char *parent_mangled)
{
  Hash *entry = NewHash();
  String *name = NewString(_name);
  Setattr(entry, NAME, Swig_scopename_last(name));
  Setattr(entry, NAME_MANGLED, Swig_name_mangle(name));
  Setattr(entry, PARENT, NewString(parent));
  Setattr(entry, PARENT_MANGLED, NewString(parent_mangled));

  Delete(name);
  return entry;
}

/**********************************************************************
 * JavascriptCore: JSEmitter implementation for JavascriptCore engine
 **********************************************************************/

class JSCEmitter : public JSEmitter
{

public:
  JSCEmitter();
  virtual ~JSCEmitter();
  virtual int initialize(Node *n);
  virtual int dump(Node *n);
  virtual int close();

protected:
  virtual int enterVariable(Node *n);
  virtual int exitVariable(Node *n);
  virtual int enterFunction(Node *n);
  virtual int exitFunction(Node *n);
  virtual int enterClass(Node *n);
  virtual int exitClass(Node *n);
  virtual void marshalInputArgs(Node *n, ParmList *parms, Wrapper *wrapper, MarshallingMode mode, bool is_member, bool is_static);
  virtual Hash *createNamespaceEntry(const char *name, const char *parent, const char *parent_mangled);
  virtual int emitNamespaces();

private:
  String *NULL_STR;
  String *VETO_SET;

  // output file and major code parts
  File *f_wrap_cpp;
  File *f_runtime;
  File *f_header;
  File *f_init;
};

JSCEmitter::JSCEmitter()
    : JSEmitter(JSEmitter::JavascriptCore), NULL_STR(NewString("NULL")), VETO_SET(NewString("JS_veto_set_variable")), f_wrap_cpp(NULL), f_runtime(NULL), f_header(NULL), f_init(NULL)
{
}

JSCEmitter::~JSCEmitter()
{
  Delete(NULL_STR);
  Delete(VETO_SET);
}

/* ---------------------------------------------------------------------
 * marshalInputArgs()
 *
 * Process all of the arguments passed into the argv array
 * and convert them into C/C++ function arguments using the
 * supplied typemaps.
 * --------------------------------------------------------------------- */

void JSCEmitter::marshalInputArgs(Node *n, ParmList *parms, Wrapper *wrapper, MarshallingMode mode, bool is_member, bool is_static)
{
  Parm *p;
  String *tm;

  // determine an offset index, as members have an extra 'this' argument
  // except: static members and ctors.
  int startIdx = 0;
  if (is_member && !is_static && mode != Ctor)
  {
    startIdx = 1;
  }
  // store number of arguments for argument checks
  int num_args = emit_num_arguments(parms) - startIdx;
  String *argcount = NewString("");
  Printf(argcount, "%d", num_args);
  Setattr(n, ARGCOUNT, argcount);

  // process arguments
  int i = 0;
  for (p = parms; p; i++)
  {
    String *arg = NewString("");
    String *type = Getattr(p, "type");

    // ignore varargs
    if (SwigType_isvarargs(type))
      break;

    switch (mode)
    {
    case Getter:
    case Function:
      if (is_member && !is_static && i == 0)
      {
        Printv(arg, "thisObject", 0);
      }
      else
      {
        Printf(arg, "argv[%d]", i - startIdx);
      }
      break;
    case Setter:
      if (is_member && !is_static && i == 0)
      {
        Printv(arg, "thisObject", 0);
      }
      else
      {
        Printv(arg, "value", 0);
      }
      break;
    case Ctor:
      Printf(arg, "argv[%d]", i);
      break;
    default:
      throw "Illegal state.";
    }
    tm = emitInputTypemap(n, p, wrapper, arg);
    Delete(arg);
    if (tm)
    {
      p = Getattr(p, "tmap:in:next");
    }
    else
    {
      p = nextSibling(p);
    }
  }
}

int JSCEmitter::initialize(Node *n)
{

  JSEmitter::initialize(n);

  /* Get the output file name */
  String *outfile = Getattr(n, "outfile");

  /* Initialize I/O */
  f_wrap_cpp = NewFile(outfile, "w", SWIG_output_files());
  if (!f_wrap_cpp)
  {
    FileErrorDisplay(outfile);
    SWIG_exit(EXIT_FAILURE);
  }

  /* Initialization of members */
  f_runtime = NewString("");
  f_init = NewString("");
  f_header = NewString("");

  state.globals(CREATE_NAMESPACES, NewString(""));
  state.globals(REGISTER_NAMESPACES, NewString(""));
  state.globals(INITIALIZER, NewString(""));

  /* Register file targets with the SWIG file handler */
  Swig_register_filebyname("begin", f_wrap_cpp);
  Swig_register_filebyname("header", f_header);
  Swig_register_filebyname("wrapper", f_wrappers);
  Swig_register_filebyname("runtime", f_runtime);
  Swig_register_filebyname("init", f_init);

  Swig_banner(f_wrap_cpp);

  return SWIG_OK;
}

int JSCEmitter::dump(Node *n)
{
  /* Get the module name */
  String *module = Getattr(n, "name");

  Template initializer_define(getTemplate("js_initializer_define"));
  initializer_define.replace("$jsname", module).pretty_print(f_header);

  SwigType_emit_type_table(f_runtime, f_wrappers);

  Printv(f_wrap_cpp, f_runtime, "\n", 0);
  Printv(f_wrap_cpp, f_header, "\n", 0);
  Printv(f_wrap_cpp, f_wrappers, "\n", 0);

  emitNamespaces();

  // compose the initializer function using a template
  Template initializer(getTemplate("js_initializer"));
  initializer.replace("$jsname", module)
      .replace("$jsregisterclasses", state.globals(INITIALIZER))
      .replace("$jscreatenamespaces", state.globals(CREATE_NAMESPACES))
      .replace("$jsregisternamespaces", state.globals(REGISTER_NAMESPACES))
      .pretty_print(f_init);

  Printv(f_wrap_cpp, f_init, 0);

  return SWIG_OK;
}

int JSCEmitter::close()
{
  Delete(f_runtime);
  Delete(f_header);
  Delete(f_wrappers);
  Delete(f_init);
  Delete(namespaces);
  Delete(f_wrap_cpp);
  return SWIG_OK;
}

int JSCEmitter::enterFunction(Node *n)
{

  JSEmitter::enterFunction(n);

  return SWIG_OK;
}

int JSCEmitter::exitFunction(Node *n)
{
  Template t_function = getTemplate("jsc_function_declaration");

  bool is_member = GetFlag(n, "ismember") != 0 || GetFlag(n, "feature:extend") != 0;
  bool is_overloaded = GetFlag(n, "sym:overloaded") != 0;

  // handle overloaded functions
  if (is_overloaded)
  {
    if (!Getattr(n, "sym:nextSibling"))
    {
      //state.function(WRAPPER_NAME, Swig_name_wrapper(Getattr(n, "name")));
      // create dispatcher
      emitFunctionDispatcher(n, is_member);
    }
    else
    {
      //don't register wrappers of overloaded functions in function tables
      return SWIG_OK;
    }
  }

  t_function.replace("$jsname", state.function(NAME))
      .replace("$jswrapper", state.function(WRAPPER_NAME));

  if (is_member)
  {
    if (GetFlag(state.function(), IS_STATIC))
    {
      t_function.pretty_print(state.clazz(STATIC_FUNCTIONS));
    }
    else
    {
      t_function.pretty_print(state.clazz(MEMBER_FUNCTIONS));
    }
  }
  else
  {
    t_function.pretty_print(Getattr(current_namespace, "functions"));
  }

  return SWIG_OK;
}

int JSCEmitter::enterVariable(Node *n)
{
  JSEmitter::enterVariable(n);
  state.variable(GETTER, NULL_STR);
  state.variable(SETTER, VETO_SET);
  return SWIG_OK;
}

int JSCEmitter::exitVariable(Node *n)
{
  Template t_variable(getTemplate("jsc_variable_declaration"));
  t_variable.replace("$jsname", state.variable(NAME))
      .replace("$jsgetter", state.variable(GETTER))
      .replace("$jssetter", state.variable(SETTER));

  if (GetFlag(n, "ismember"))
  {
    if (GetFlag(state.variable(), IS_STATIC) || Equal(Getattr(n, "nodeType"), "enumitem"))
    {
      t_variable.pretty_print(state.clazz(STATIC_VARIABLES));
    }
    else
    {
      t_variable.pretty_print(state.clazz(MEMBER_VARIABLES));
    }
  }
  else
  {
    t_variable.pretty_print(Getattr(current_namespace, "values"));
  }

  return SWIG_OK;
}

int JSCEmitter::enterClass(Node *n)
{
  JSEmitter::enterClass(n);
  state.clazz(MEMBER_VARIABLES, NewString(""));
  state.clazz(MEMBER_FUNCTIONS, NewString(""));
  state.clazz(STATIC_VARIABLES, NewString(""));
  state.clazz(STATIC_FUNCTIONS, NewString(""));

  Template t_class_decl = getTemplate("jsc_class_declaration");
  t_class_decl.replace("$jsmangledname", state.clazz(NAME_MANGLED))
      .pretty_print(f_wrappers);

  return SWIG_OK;
}

int JSCEmitter::exitClass(Node *n)
{
  Template t_class_tables(getTemplate("jsc_class_tables"));
  t_class_tables.replace("$jsmangledname", state.clazz(NAME_MANGLED))
      .replace("$jsclassvariables", state.clazz(MEMBER_VARIABLES))
      .replace("$jsclassfunctions", state.clazz(MEMBER_FUNCTIONS))
      .replace("$jsstaticclassfunctions", state.clazz(STATIC_FUNCTIONS))
      .replace("$jsstaticclassvariables", state.clazz(STATIC_VARIABLES))
      .pretty_print(f_wrappers);

  /* adds the ctor wrappers at this position */
  // Note: this is necessary to avoid extra forward declarations.
  //Append(f_wrappers, state.clazz(CTOR_WRAPPERS));

  // for abstract classes add a vetoing ctor
  if (GetFlag(state.clazz(), IS_ABSTRACT))
  {
    Template t_veto_ctor(getTemplate("js_veto_ctor"));
    t_veto_ctor.replace("$jswrapper", state.clazz(CTOR))
        .replace("$jsname", state.clazz(NAME))
        .pretty_print(f_wrappers);
  }

  /* adds a class template statement to initializer function */
  Template t_classtemplate(getTemplate("jsc_class_definition"));

  /* prepare registration of base class */
  String *jsclass_inheritance = NewString("");
  Node *base_class = getBaseClass(n);
  if (base_class != NULL)
  {
    Template t_inherit(getTemplate("jsc_class_inherit"));
    t_inherit.replace("$jsmangledname", state.clazz(NAME_MANGLED))
        .replace("$jsbaseclassmangled", SwigType_manglestr(Getattr(base_class, "name")))
        .pretty_print(jsclass_inheritance);
  }
  else
  {
    Template t_inherit(getTemplate("jsc_class_noinherit"));
    t_inherit.replace("$jsmangledname", state.clazz(NAME_MANGLED))
        .pretty_print(jsclass_inheritance);
  }

  t_classtemplate.replace("$jsmangledname", state.clazz(NAME_MANGLED))
      .replace("$jsmangledtype", state.clazz(TYPE_MANGLED))
      .replace("$jsclass_inheritance", jsclass_inheritance)
      .replace("$jsctor", state.clazz(CTOR))
      .replace("$jsdtor", state.clazz(DTOR))
      .pretty_print(state.globals(INITIALIZER));
  Delete(jsclass_inheritance);

  /* Note: this makes sure that there is a swig_type added for this class */
  SwigType_remember_clientdata(state.clazz(TYPE_MANGLED), NewString("0"));

  /* adds a class registration statement to initializer function */
  Template t_registerclass(getTemplate("jsc_class_registration"));
  t_registerclass.replace("$jsname", state.clazz(NAME))
      .replace("$jsmangledname", state.clazz(NAME_MANGLED))
      .replace("$jsnspace", Getattr(state.clazz("nspace"), NAME_MANGLED))
      .pretty_print(state.globals(INITIALIZER));

  return SWIG_OK;
}

Hash *JSCEmitter::createNamespaceEntry(const char *name, const char *parent, const char *parent_mangled)
{
  Hash *entry = JSEmitter::createNamespaceEntry(name, parent, parent_mangled);
  Setattr(entry, "functions", NewString(""));
  Setattr(entry, "values", NewString(""));
  return entry;
}

int JSCEmitter::emitNamespaces()
{
  Iterator it;
  for (it = First(namespaces); it.item; it = Next(it))
  {
    Hash *entry = it.item;
    String *name = Getattr(entry, NAME);
    String *name_mangled = Getattr(entry, NAME_MANGLED);
    String *parent_mangled = Getattr(entry, PARENT_MANGLED);
    String *functions = Getattr(entry, "functions");
    String *variables = Getattr(entry, "values");

    // skip the global namespace which is given by the application

    Template namespace_definition(getTemplate("jsc_nspace_declaration"));
    namespace_definition.replace("$jsglobalvariables", variables)
        .replace("$jsglobalfunctions", functions)
        .replace("$jsnspace", name_mangled)
        .replace("$jsmangledname", name_mangled)
        .pretty_print(f_wrap_cpp);

    Template t_createNamespace(getTemplate("jsc_nspace_definition"));
    t_createNamespace.replace("$jsmangledname", name_mangled);
    Append(state.globals(CREATE_NAMESPACES), t_createNamespace.str());

    // Don't register 'exports' as namespace. It is return to the application.
    if (!Equal("exports", name))
    {
      Template t_registerNamespace(getTemplate("jsc_nspace_registration"));
      t_registerNamespace.replace("$jsmangledname", name_mangled)
          .replace("$jsname", name)
          .replace("$jsparent", parent_mangled);
      Append(state.globals(REGISTER_NAMESPACES), t_registerNamespace.str());
    }
  }

  return SWIG_OK;
}

JSEmitter *swig_javascript_create_JSCEmitter()
{
  return new JSCEmitter();
}

/**********************************************************************
 * V8: JSEmitter implementation for V8 engine
 **********************************************************************/

class V8Emitter : public JSEmitter
{

public:
  V8Emitter();

  virtual ~V8Emitter();
  virtual int initialize(Node *n);
  virtual int dump(Node *n);
  virtual int close();
  virtual int enterClass(Node *n);
  virtual int exitClass(Node *n);
  virtual int enterVariable(Node *n);
  virtual int exitVariable(Node *n);
  virtual int exitFunction(Node *n);

protected:
  virtual void marshalInputArgs(Node *n, ParmList *parms, Wrapper *wrapper, MarshallingMode mode, bool is_member, bool is_static);
  virtual int emitNamespaces();

protected:
  /* built-in parts */
  String *f_runtime;
  String *f_header;
  String *f_init;
  String *f_post_init;

  /* part for class templates */
  String *f_class_templates;

  /* parts for initilizer */
  String *f_init_namespaces;
  String *f_init_class_templates;
  String *f_init_wrappers;
  String *f_init_inheritance;
  String *f_init_class_instances;
  String *f_init_static_wrappers;
  String *f_init_register_classes;
  String *f_init_register_namespaces;

  // the output cpp file
  File *f_wrap_cpp;

  String *NULL_STR;
  String *VETO_SET;
  String *moduleName;
};

V8Emitter::V8Emitter()
    : JSEmitter(JSEmitter::V8), NULL_STR(NewString("0")), VETO_SET(NewString("JS_veto_set_variable"))
{
}

V8Emitter::~V8Emitter()
{
  Delete(NULL_STR);
  Delete(VETO_SET);
}

int V8Emitter::initialize(Node *n)
{
  Printf(stdout, "Initialize V8Emitter\n");
  JSEmitter::initialize(n);

  moduleName = Getattr(n, "name");

  // Get the output file name
  String *outfile = Getattr(n, "outfile");
  f_wrap_cpp = NewFile(outfile, "w", SWIG_output_files());
  if (!f_wrap_cpp)
  {
    FileErrorDisplay(outfile);
    SWIG_exit(EXIT_FAILURE);
  }

  f_runtime = NewString("");
  f_header = NewString("");
  f_class_templates = NewString("");
  f_init = NewString("");
  f_post_init = NewString("");

  f_init_namespaces = NewString("");
  f_init_class_templates = NewString("");
  f_init_wrappers = NewString("");
  f_init_inheritance = NewString("");
  f_init_class_instances = NewString("");
  f_init_static_wrappers = NewString("");
  f_init_register_classes = NewString("");
  f_init_register_namespaces = NewString("");

  // note: this is necessary for built-in generation of SWIG runtime code
  Swig_register_filebyname("begin", f_wrap_cpp);
  Swig_register_filebyname("runtime", f_runtime);
  Swig_register_filebyname("header", f_header);
  Swig_register_filebyname("wrapper", f_wrappers);
  Swig_register_filebyname("init", f_init);
  Swig_register_filebyname("post-init", f_post_init);

  state.globals(FORCE_CPP, NewString("1"));

  Swig_banner(f_wrap_cpp);

  return SWIG_OK;
}

int V8Emitter::dump(Node *n)
{
  /* Get the module name */
  String *module = Getattr(n, "name");

  Template initializer_define(getTemplate("js_initializer_define"));
  initializer_define.replace("$jsname", module).pretty_print(f_header);

  SwigType_emit_type_table(f_runtime, f_wrappers);

  Printv(f_wrap_cpp, f_runtime, "\n", 0);
  Printv(f_wrap_cpp, f_header, "\n", 0);
  Printv(f_wrap_cpp, f_class_templates, "\n", 0);
  Printv(f_wrap_cpp, f_wrappers, "\n", 0);

  emitNamespaces();

  // compose the initializer function using a template
  // filled with sub-parts
  Template initializer(getTemplate("js_initializer"));
  initializer.replace("$jsname", moduleName)
      .replace("$jsv8nspaces", f_init_namespaces)
      .replace("$jsv8classtemplates", f_init_class_templates)
      .replace("$jsv8wrappers", f_init_wrappers)
      .replace("$jsv8inheritance", f_init_inheritance)
      .replace("$jsv8classinstances", f_init_class_instances)
      .replace("$jsv8staticwrappers", f_init_static_wrappers)
      .replace("$jsv8registerclasses", f_init_register_classes)
      .replace("$jsv8registernspaces", f_init_register_namespaces);
  Printv(f_init, initializer.str(), 0);

  Printv(f_wrap_cpp, f_init, 0);

  Printv(f_wrap_cpp, f_post_init, 0);

  return SWIG_OK;
}

int V8Emitter::close()
{
  Delete(f_runtime);
  Delete(f_header);
  Delete(f_class_templates);
  Delete(f_init_namespaces);
  Delete(f_init_class_templates);
  Delete(f_init_wrappers);
  Delete(f_init_inheritance);
  Delete(f_init_class_instances);
  Delete(f_init_static_wrappers);
  Delete(f_init_register_classes);
  Delete(f_init_register_namespaces);
  Delete(f_init);
  Delete(f_post_init);
  Delete(f_wrap_cpp);
  return SWIG_OK;
}

int V8Emitter::enterClass(Node *n)
{
  JSEmitter::enterClass(n);

  // emit declaration of a v8 class template
  Template t_decl_class(getTemplate("jsv8_declare_class_template"));
  t_decl_class.replace("$jsmangledname", state.clazz(NAME_MANGLED))
      .trim()
      .pretty_print(f_class_templates);

  return SWIG_OK;
}

int V8Emitter::exitClass(Node *n)
{
  if (GetFlag(state.clazz(), IS_ABSTRACT))
  {
    Template t_veto_ctor(getTemplate("js_veto_ctor"));
    t_veto_ctor.replace("$jswrapper", state.clazz(CTOR))
        .replace("$jsname", state.clazz(NAME))
        .pretty_print(f_wrappers);
  }

  /* Note: this makes sure that there is a swig_type added for this class */
  String *clientData = NewString("");
  Printf(clientData, "&%s_clientData", state.clazz(NAME_MANGLED));

  /* Note: this makes sure that there is a swig_type added for this class */
  SwigType_remember_clientdata(state.clazz(TYPE_MANGLED), NewString("0"));

  // emit definition of v8 class template
  Template t_def_class = getTemplate("jsv8_define_class_template");
  t_def_class.replace("$jsmangledname", state.clazz(NAME_MANGLED))
      .replace("$jsname", state.clazz(NAME))
      .replace("$jsmangledtype", state.clazz(TYPE_MANGLED))
      .replace("$jsdtor", state.clazz(DTOR))
      .trim()
      .pretty_print(f_init_class_templates);

  Template t_class_instance = getTemplate("jsv8_create_class_instance");
  t_class_instance.replace("$jsname", state.clazz(NAME))
      .replace("$jsmangledname", state.clazz(NAME_MANGLED))
      .replace("$jsctor", state.clazz(CTOR))
      .trim()
      .pretty_print(f_init_class_instances);

  //  emit inheritance setup
  Node *baseClass = getBaseClass(n);
  if (baseClass)
  {
    String *base_name = Getattr(baseClass, "name");

    Template t_inherit = getTemplate("jsv8_inherit");

    String *base_name_mangled = SwigType_manglestr(base_name);
    t_inherit.replace("$jsmangledname", state.clazz(NAME_MANGLED))
        .replace("$jsbaseclass", base_name_mangled)
        .trim()
        .pretty_print(f_init_inheritance);
    Delete(base_name_mangled);
  }
  //  emit registration of class template
  Template t_register = getTemplate("jsv8_register_class");
  t_register.replace("$jsmangledname", state.clazz(NAME_MANGLED))
      .replace("$jsname", state.clazz(NAME))
      .replace("$jsparent", Getattr(state.clazz("nspace"), NAME_MANGLED))
      .trim()
      .pretty_print(f_init_register_classes);

  return SWIG_OK;
}

int V8Emitter::enterVariable(Node *n)
{
  JSEmitter::enterVariable(n);

  state.variable(GETTER, NULL_STR);
  state.variable(SETTER, VETO_SET);

  return SWIG_OK;
}

int V8Emitter::exitVariable(Node *n)
{
  if (GetFlag(n, "ismember"))
  {
    if (GetFlag(state.variable(), IS_STATIC) || Equal(Getattr(n, "nodeType"), "enumitem"))
    {
      Template t_register = getTemplate("jsv8_register_static_variable");
      t_register.replace("$jsparent", state.clazz(NAME_MANGLED))
          .replace("$jsname", state.variable(NAME))
          .replace("$jsgetter", state.variable(GETTER))
          .replace("$jssetter", state.variable(SETTER))
          .trim()
          .pretty_print(f_init_static_wrappers);
    }
    else
    {
      Template t_register = getTemplate("jsv8_register_member_variable");
      t_register.replace("$jsmangledname", state.clazz(NAME_MANGLED))
          .replace("$jsname", state.variable(NAME))
          .replace("$jsgetter", state.variable(GETTER))
          .replace("$jssetter", state.variable(SETTER))
          .trim()
          .pretty_print(f_init_wrappers);
    }
  }
  else
  {
    // Note: a global variable is treated like a static variable
    //       with the parent being a nspace object (instead of class object)
    Template t_register = getTemplate("jsv8_register_static_variable");
    t_register.replace("$jsparent", Getattr(current_namespace, NAME_MANGLED))
        .replace("$jsname", state.variable(NAME))
        .replace("$jsgetter", state.variable(GETTER))
        .replace("$jssetter", state.variable(SETTER))
        .trim()
        .pretty_print(f_init_wrappers);
  }

  return SWIG_OK;
}

int V8Emitter::exitFunction(Node *n)
{
  bool is_member = GetFlag(n, "ismember") != 0 || GetFlag(n, "feature:extend") != 0;

  // create a dispatcher for overloaded functions
  bool is_overloaded = GetFlag(n, "sym:overloaded") != 0;
  if (is_overloaded)
  {
    if (!Getattr(n, "sym:nextSibling"))
    {
      //state.function(WRAPPER_NAME, Swig_name_wrapper(Getattr(n, "name")));
      emitFunctionDispatcher(n, is_member);
    }
    else
    {
      //don't register wrappers of overloaded functions in function tables
      return SWIG_OK;
    }
  }
  // register the function at the specific context
  if (is_member)
  {
    if (GetFlag(state.function(), IS_STATIC))
    {
      Template t_register = getTemplate("jsv8_register_static_function");
      t_register.replace("$jsparent", state.clazz(NAME_MANGLED))
          .replace("$jsname", state.function(NAME))
          .replace("$jswrapper", state.function(WRAPPER_NAME))
          .trim()
          .pretty_print(f_init_static_wrappers);
    }
    else
    {
      Template t_register = getTemplate("jsv8_register_member_function");
      t_register.replace("$jsmangledname", state.clazz(NAME_MANGLED))
          .replace("$jsname", state.function(NAME))
          .replace("$jswrapper", state.function(WRAPPER_NAME))
          .trim()
          .pretty_print(f_init_wrappers);
    }
  }
  else
  {
    // Note: a global function is treated like a static function
    //       with the parent being a nspace object instead of class object
    Template t_register = getTemplate("jsv8_register_static_function");
    t_register.replace("$jsparent", Getattr(current_namespace, NAME_MANGLED))
        .replace("$jsname", state.function(NAME))
        .replace("$jswrapper", state.function(WRAPPER_NAME))
        .trim()
        .pretty_print(f_init_static_wrappers);
  }

  return SWIG_OK;
}

void V8Emitter::marshalInputArgs(Node *n, ParmList *parms, Wrapper *wrapper, MarshallingMode mode, bool is_member, bool is_static)
{
  Parm *p;
  String *tm;

  int startIdx = 0;
  if (is_member && !is_static && mode != Ctor)
  {
    startIdx = 1;
  }
  // store number of arguments for argument checks
  int num_args = emit_num_arguments(parms) - startIdx;
  String *argcount = NewString("");
  Printf(argcount, "%d", num_args);
  Setattr(n, ARGCOUNT, argcount);

  int i = 0;
  for (p = parms; p; i++)
  {
    String *arg = NewString("");
    String *type = Getattr(p, "type");

    // ignore varargs
    if (SwigType_isvarargs(type))
      break;

    switch (mode)
    {
    case Getter:
      if (is_member && !is_static && i == 0)
      {
        Printv(arg, "info.Holder()", 0);
      }
      else
      {
        Printf(arg, "args[%d]", i - startIdx);
      }
      break;
    case Function:
      if (is_member && !is_static && i == 0)
      {
        Printv(arg, "args.Holder()", 0);
      }
      else
      {
        Printf(arg, "args[%d]", i - startIdx);
      }
      break;
    case Setter:
      if (is_member && !is_static && i == 0)
      {
        Printv(arg, "info.Holder()", 0);
      }
      else
      {
        Printv(arg, "value", 0);
      }
      break;
    case Ctor:
      Printf(arg, "args[%d]", i);
      break;
    default:
      throw "Illegal state.";
    }

    tm = emitInputTypemap(n, p, wrapper, arg);
    Delete(arg);

    if (tm)
    {
      p = Getattr(p, "tmap:in:next");
    }
    else
    {
      p = nextSibling(p);
    }
  }
}

int V8Emitter::emitNamespaces()
{
  Iterator it;
  for (it = First(namespaces); it.item; it = Next(it))
  {
    Hash *entry = it.item;
    String *name = Getattr(entry, NAME);
    String *name_mangled = Getattr(entry, NAME_MANGLED);
    String *parent = Getattr(entry, PARENT);
    String *parent_mangled = Getattr(entry, PARENT_MANGLED);

    bool do_create = true;
    bool do_register = true;

    if (Equal(parent, ""))
    {
      do_register = false;
    }
    // Note: 'exports' is by convention the name of the object where
    // globals are stored into
    if (Equal(name, "exports"))
    {
      do_create = false;
    }

    if (do_create)
    {
      // create namespace object and register it to the parent scope
      Template t_create_ns = getTemplate("jsv8_create_namespace");
      t_create_ns.replace("$jsmangledname", name_mangled)
          .trim()
          .pretty_print(f_init_namespaces);
    }

    if (do_register)
    {
      Template t_register_ns = getTemplate("jsv8_register_namespace");
      t_register_ns.replace("$jsmangledname", name_mangled)
          .replace("$jsname", name)
          .replace("$jsparent", parent_mangled)
          .trim();

      // prepend in order to achieve reversed order of registration statements
      String *tmp_register_stmt = NewString("");
      t_register_ns.pretty_print(tmp_register_stmt);
      Insert(f_init_register_namespaces, 0, tmp_register_stmt);
      Delete(tmp_register_stmt);
    }
  }

  return SWIG_OK;
}

JSEmitter *swig_javascript_create_V8Emitter()
{
  return new V8Emitter();
}

/**********************************************************************
 * Helper implementations
 **********************************************************************/

JSEmitterState::JSEmitterState()
    : globalHash(NewHash())
{
  // initialize sub-hashes
  Setattr(globalHash, "class", NewHash());
  Setattr(globalHash, "function", NewHash());
  Setattr(globalHash, "variable", NewHash());
}

JSEmitterState::~JSEmitterState()
{
  Delete(globalHash);
}

DOH *JSEmitterState::getState(const char *key, bool new_key)
{
  if (new_key)
  {
    Hash *hash = NewHash();
    Setattr(globalHash, key, hash);
  }
  return Getattr(globalHash, key);
}

DOH *JSEmitterState::globals()
{
  return globalHash;
}

DOH *JSEmitterState::globals(const char *key, DOH *initial)
{
  if (initial != 0)
  {
    Setattr(globalHash, key, initial);
  }
  return Getattr(globalHash, key);
}

DOH *JSEmitterState::clazz(bool new_key)
{
  return getState("class", new_key);
}

DOH *JSEmitterState::clazz(const char *key, DOH *initial)
{
  DOH *c = clazz();
  if (initial != 0)
  {
    Setattr(c, key, initial);
  }
  return Getattr(c, key);
}

DOH *JSEmitterState::function(bool new_key)
{
  return getState("function", new_key);
}

DOH *JSEmitterState::function(const char *key, DOH *initial)
{
  DOH *f = function();
  if (initial != 0)
  {
    Setattr(f, key, initial);
  }
  return Getattr(f, key);
}

DOH *JSEmitterState::variable(bool new_key)
{
  return getState("variable", new_key);
}

DOH *JSEmitterState::variable(const char *key, DOH *initial)
{
  DOH *v = variable();
  if (initial != 0)
  {
    Setattr(v, key, initial);
  }
  return Getattr(v, key);
}

/*static*/
int JSEmitterState::IsSet(DOH *val)
{
  if (!val)
  {
    return 0;
  }
  else
  {
    const char *cval = Char(val);
    if (!cval)
      return 0;
    return (strcmp(cval, "0") != 0) ? 1 : 0;
  }
}

/* -----------------------------------------------------------------------------
 * Template::Template() :  creates a Template class for given template code
 * ----------------------------------------------------------------------------- */

Template::Template(const String *code_)
{

  if (!code_)
  {
    Printf(stdout, "Template code was null. Illegal input for template.");
    SWIG_exit(EXIT_FAILURE);
  }
  code = NewString(code_);
  templateName = NewString("");
}

Template::Template(const String *code_, const String *templateName_)
{

  if (!code_)
  {
    Printf(stdout, "Template code was null. Illegal input for template.");
    SWIG_exit(EXIT_FAILURE);
  }

  code = NewString(code_);
  templateName = NewString(templateName_);
}

/* -----------------------------------------------------------------------------
 * Template::~Template() :  cleans up of Template.
 * ----------------------------------------------------------------------------- */

Template::~Template()
{
  Delete(code);
  Delete(templateName);
}

/* -----------------------------------------------------------------------------
 * String* Template::str() :  retrieves the current content of the template.
 * ----------------------------------------------------------------------------- */

String *Template::str()
{
  if (js_template_enable_debug)
  {
    String *pre_code = NewString("");
    String *post_code = NewString("");
    String *debug_code = NewString("");
    Printf(pre_code, "/* begin fragment(\"%s\") */", templateName);
    Printf(post_code, "/* end fragment(\"%s\") */", templateName);
    Printf(debug_code, "%s\n%s\n%s\n", pre_code, code, post_code);

    Delete(code);
    Delete(pre_code);
    Delete(post_code);

    code = debug_code;
  }
  return code;
}

Template &Template::trim()
{
  const char *str = Char(code);
  if (str == 0)
    return *this;

  int length = Len(code);
  if (length == 0)
    return *this;

  int idx;
  for (idx = 0; idx < length; ++idx)
  {
    if (str[idx] != ' ' && str[idx] != '\t' && str[idx] != '\r' && str[idx] != '\n')
      break;
  }
  int start_pos = idx;

  for (idx = length - 1; idx >= start_pos; --idx)
  {
    if (str[idx] != ' ' && str[idx] != '\t' && str[idx] != '\r' && str[idx] != '\n')
      break;
  }
  int end_pos = idx;

  int new_length = end_pos - start_pos + 1;
  char *newstr = new char[new_length + 1];
  memcpy(newstr, str + start_pos, new_length);
  newstr[new_length] = 0;

  Delete(code);
  code = NewString(newstr);
  delete[] newstr;

  return *this;
}

/* -----------------------------------------------------------------------------
 * Template&  Template::replace(const String* pattern, const String* repl) :
 *
 *  replaces all occurrences of a given pattern with a given replacement.
 *
 *  - pattern:  the pattern to be replaced
 *  - repl:     the replacement string
 *  - returns a reference to the Template to allow chaining of methods.
 * ----------------------------------------------------------------------------- */

Template &Template::replace(const String *pattern, const String *repl)
{
  Replaceall(code, pattern, repl);
  return *this;
}

Template &Template::print(DOH *doh)
{
  Printv(doh, str(), 0);
  return *this;
}

Template &Template::pretty_print(DOH *doh)
{
  Wrapper_pretty_print(str(), doh);
  return *this;
}

Template::Template(const Template &t)
{
  code = NewString(t.code);
  templateName = NewString(t.templateName);
}

void Template::operator=(const Template &t)
{
  Delete(code);
  Delete(templateName);
  code = NewString(t.code);
  templateName = NewString(t.templateName);
}

// TODO : Check eventual memory leaks
ProxyInterface::~ProxyInterface()
{
  // Delete(classFileName);
  // Delete(classFilePath);
  // Delete(proxyExtraCode);
  // Delete(empty_string);
}

/**
 * Generate the code of the interface using the collected information
 */
void ProxyInterface::generateProxy()
{
  // className = Getattr(n, "sym:name");
  String *classNameKebabCase = Swig_string_kcase(className);
  classFileName = NewStringf("%s.d.ts", classNameKebabCase);
  classFilePath = NewStringf("%s%s", SWIG_output_directory(), classFileName);
  classFilePtr = NewFile(classFilePath, "w", SWIG_output_files());

  String *proxyTypeName;
  switch (proxyType)
  {
  case classType:
    proxyTypeName = NewString("class");
    break;
  case interfaceType:
    proxyTypeName = NewString("interface");
    break;
  case enumType:
    proxyTypeName = NewString("declare enum");
    break;
  }

  Printf(classFilePtr, "%s %s ", proxyTypeName, className);

  if (baseClassName && Strcmp(baseClassName, "") != 0)
  {
    Printf(classFilePtr, "extends %s ", baseClassName);
    Delete(baseClassName);
  }
  Printf(classFilePtr, "{\n");
  Printf(classFilePtr, "%s", classCode);
  Printf(classFilePtr, "%s", proxyExtraCode);
  Printf(classFilePtr, "}\n");
}

/**
 * Add a class member variable
 *
 * @param n The node where the public variable is declared
 * @param typescriptType The proper type TypeScript type to be added to the member declaration
 */
void ProxyInterface::addMemberVariable(Node *n, String *typescriptType)
{
  String *typeFromTypemap;
  String *memberVariableType = NewString(typescriptType);
  // if ((typeFromTypemap = Swig_typemap_lookup("typescripttype", n, "", 0)))
  // {
  //   Printf(memberVariableType, "%s", typeFromTypemap);
  // }
  // else
  // {
  //   Printf(memberVariableType, "%s", Getattr(n, "type"));
  // }
  Printf(stdout, "   %s : %s;\n", Getattr(n, "sym:name"), memberVariableType);
  Printf(classCode, "   %s : %s;\n", Getattr(n, "sym:name"), memberVariableType);
  Printf(stdout, "  %%%%%%%%%%%%%%%%%%%%%%%%%% %s : %s;\n", Getattr(n, "sym:name"), memberVariableType);

  // Delete(memberVariableType);
}

/**
 * Add an enum value
 *
 * @param n The node where the enum value is defined
 * @param value The value of the enum
 */
void ProxyInterface::addEnumValue(Node *n, String *value)
{
  if (value)
  {
    Printf(classCode, "   %s = %s,\n", Getattr(n, "sym:name"), value);
  }
  else
  {
    Printf(classCode, "   %s,\n", Getattr(n, "sym:name"));
  }
}

/**
 * Add code found as attribute of the node to the proxy interface
 * In some types, like std::vector and(or) std::map, the 
 *
 * @param code The code in TypeScript to be added in the interface
 */
void ProxyInterface::insertCode(String *code)
{
  Printf(proxyExtraCode, "%s", code);
}
