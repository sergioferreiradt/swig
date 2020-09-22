
#include "swigmod.h"

class TYPESCRIPT : public Language
{
public:
    static const char *usage;

    virtual void main(int argc, char *argv[]);
    virtual int top(Node *n);
    virtual int functionWrapper(Node *n);
    virtual int variableWrapper(Node *n);
    virtual int membervariableHandler(Node *);
    virtual int classHandler(Node *n);

protected:
    /* built-in parts */
    String *f_runtime;
    String *f_header;
    String *f_init;
    String *f_post_init;
    String *f_class_templates;
    File *f_wrap_cpp;
    File *f_wrappers;

    String *classCode;

    /* parts for initilizer */
    // String *f_init_namespaces;
    // String *f_init_class_templates;
    // String *f_init_wrappers;
    // String *f_init_inheritance;
    // String *f_init_class_instances;
    // String *f_init_static_wrappers;
    // String *f_init_register_classes;
    // String *f_init_register_namespaces;

private:
    String *getBaseClass(Node *n);
};

void TYPESCRIPT::main(int argc, char *argv[])
{
    printf("I'm the Typescript module.\n");
    /* Set language-specific subdirectory in SWIG library */
    SWIG_library_directory("typescript");
    /* Set language-specific preprocessing symbol */
    Preprocessor_define("SWIGJAVASCRIPT 1", 0);
    /* Set language-specific configuration file */
    SWIG_config_file("javascript.swg");
    /* Set typemap language (historical) */
    SWIG_typemap_lang("javascript");
}

int TYPESCRIPT::top(Node *n)
{
    String *module = Getattr(n, "name");
    Printf(stdout, "Module : %s\n", module);

    f_runtime = NewString("");
    f_header = NewString("");
    f_class_templates = NewString("");
    f_init = NewString("");
    f_post_init = NewString("");

    classCode = NewString("");

    /* Register file targets with the SWIG file handler */
    // note: this is necessary for built-in generation of SWIG runtime code
    Swig_register_filebyname("begin", f_wrap_cpp);
    Swig_register_filebyname("runtime", f_runtime);
    Swig_register_filebyname("header", f_header);
    Swig_register_filebyname("wrapper", f_wrappers);
    Swig_register_filebyname("init", f_init);
    Swig_register_filebyname("post-init", f_post_init);

    Language::top(n);
    return SWIG_OK;
}

extern "C" Language *
swig_typescript(void)
{
    return new TYPESCRIPT();
}

// Executed when ind a class member function
int TYPESCRIPT::functionWrapper(Node *n)
{
    // Printf(stdout," Typescript Function Wrapper : %s\n", Getattr(n,"sym:name"));
    return SWIG_OK;
}

// Executed when find a static variable
int TYPESCRIPT::variableWrapper(Node *n)
{
    Printf(stdout, "     Typescript Variable Wrapper : %s\n", Getattr(n, "sym:name"));
    return SWIG_OK;
}

// Executed when find a public member variable
int TYPESCRIPT::membervariableHandler(Node *n)
{
    // Printf(stdout, "     Typescript Variable Handler : %s\n", Getattr(n, "sym:name"));
    // Printf(stdout, "      ===== TYPE : %s \n", Getattr(n, "type"));
    String *typeFromTypemap;
    String *memberVariableType = NewString("");
    if ((typeFromTypemap = Swig_typemap_lookup("typescripttype", n, "", 0)))
    {
      Printf(memberVariableType, "%s", typeFromTypemap);
    } else {
        Printf(memberVariableType, "%s", Getattr(n, "type"));
    }
    Printf(classCode, "   %s : %s;\n", Getattr(n, "sym:name"), memberVariableType);
    Printf(stdout, "  %%%%%%%%%%%%%%%%%%%%%%%%%% %s : %s;\n", Getattr(n, "sym:name"), memberVariableType);

    Delete(memberVariableType);
    return SWIG_OK;
}

int TYPESCRIPT::classHandler(Node *n)
{
    Printf(stdout, " Typescript Class Handler : %s\n", Getattr(n, "sym:name"));
    String *className = Getattr(n, "sym:name");
    // TODO - Convert the name to separated by dashes
    String *classFileName = NewStringf("%s.d.ts", className);
    String *classFilePath = NewStringf("%s%s", SWIG_output_directory(), classFileName);
    File *classFilePtr = NewFile(classFilePath, "w", SWIG_output_files());
    Language::classHandler(n);
    // TODO : Still need to have extends if that's the case
    Printf(classFilePtr, "interface %s ", className);
    String *baseClassName = getBaseClass(n);
    if ( baseClassName ) {
        Printf(classFilePtr, "extends %s ", baseClassName);
        Delete(baseClassName);
    }
    Printf(classFilePtr, "{\n");
    Printf(classFilePtr, "%s", classCode);
    Printf(classFilePtr, "}\n");

    Delete(classFileName);
    Delete(classFilePath);
    Delete(classFilePtr);

    return SWIG_OK;
}

/**
 * Obtain the information about the base class that a node extends
 *
 * @param n The node that represents a class
 * @return the string representation of the base class name
 */
String *TYPESCRIPT::getBaseClass(Node *n)
{
    SwigType *c_baseclassname = NULL;
    String *baseClassName = NULL;

    List *baselist = Getattr(n, "bases");
    if (baselist)
    {
        Iterator base = First(baselist);
        while (base.item)
        {
            if (!(GetFlag(base.item, "feature:ignore") || Getattr(base.item, "feature:interface")))
            {
                SwigType *baseclassname = Getattr(base.item, "name");
                Printf(stdout," ####### Base Classname : %s\n", baseclassname);
                baseClassName = NewString(baseclassname);
                // if (!c_baseclassname)
                // {
                //     String *name = getProxyName(baseclassname);
                //     if (name)
                //     {
                //         c_baseclassname = baseclassname;
                //         baseclass = name;
                //     }
                // }
                // else
                // {
                //     /* Warn about multiple inheritance for additional base class(es) */
                //     String *proxyclassname = Getattr(n, "classtypeobj");
                //     Swig_warning(WARN_JAVA_MULTIPLE_INHERITANCE, Getfile(n), Getline(n),
                //                  "Warning for %s, base %s ignored. Multiple inheritance is not supported in Java.\n", SwigType_namestr(proxyclassname), SwigType_namestr(baseclassname));
                // }
            }
            base = Next(base);
        }
    }
    return baseClassName;
}