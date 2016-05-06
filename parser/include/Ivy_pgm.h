//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <regex>

#include "SymbolTableEntry.h"
#include "Frame.h"
#include "location.hh"

#define INDENT 3
#define STACKSIZE (64*1024)

extern int srclineno;
extern int string_start_line;

extern std::ostream& trace_ostream;
extern bool trace_lexer;
extern bool trace_parser;
extern bool trace_evaluate;


extern std::regex identifier_regex;
extern std::regex identifier_trailing_digits_regex;
extern std::regex digits_regex;



class Xpr;
class Stmt;
class Ivy_pgm;

void init_builtin_table();

extern Ivy_pgm* p_Ivy_pgm;

class Ivy_pgm
{
// variables
private:
    char pile[STACKSIZE];
public:
    std::string ivyscript_filename;
    unsigned int executing_frame {0u}; // offset from "stack" to beginning of currently executing frame
    char* stack = pile;
    char* stacktop = pile;
    std::list<Block*> blockStack {};
        // blockStack does not "own" Block objects.
        // blockStack rises and falls as the parser runs, and is used when parsing variable or function references
        // to look up things in the symbol tables in those blocks.

        // A nested block with its constructors, code (statements), and destructors is "owned" by the Stmt_nested_Block
        // which is part of the executable syntax tree after parsing is complete.

    std::list<SymbolTableEntry*> decl_prefix_stack {};
        // For "int i,j", a "declaration prefix" SymbolTableEntry is pushed here, and this is used as a template to build the things being declared.
        // The frame offset is not used for these template STEs.
    int error_count {0};
    std::string input_file_name {};
    size_t max_stack_offset {STACKSIZE - biggest_type};
    size_t next_avail_static {0u};
    // not yet//int srclineno; // hey, make sure lex uses this one - pass PD_DOC
    size_t stack_size {STACKSIZE};
    // not yet//int string_start_line; // make sure lex uses this one - pass PD_DOC
    bool successful_compile {false};
    bool unsuccessful_compile {false};
    int warning_count {0};
    std::string compile_msg {};

// methods
    Ivy_pgm(const std::string& f);
    ~Ivy_pgm();

    Block* p_global_Block()  {if (blockStack.size() == 0) return nullptr; else return blockStack.back();}
    Block* p_current_Block() {if (blockStack.size() == 0) return nullptr; else return blockStack.front();}

    void load_program(const std::string& filename);

    void warning(const std::string&);
    void error(const std::string&);

    Frame* declare_function(std::string* id,std::list<Arg>* p_arglist);

    int chain_explicit_initializer(const yy::location&, std::pair<const std::string,SymbolTableEntry>*, Xpr*);
    void chain_default_initializer(const yy::location&, std::pair<const std::string,SymbolTableEntry>*);

    std::pair<const std::string,SymbolTableEntry>* add_variable(const std::string& id);

    std::pair<const std::string,SymbolTableEntry>* SymbolTable_search(const std::string& id);

    Xpr* build_function_call(const yy::location&, const  std::string* p_ID,std::list<Xpr*>* p_xprlist);

    void start_Block();
    void end_Block();


    Stmt* make_Stmt_return(const yy::location&, Xpr* p_xpr);

    void snapshot(std::ostream&);
};


// The ivy parser is adapted from a interpreter I wrote when I was in Odawara in 1998
// using Visual C++ as part of a Windows app that could render 3D views of worlds
// described in a C-like programming language with builting primitives to
// scale, rotate, translate, draw lines, polygons, display text in a font, etc.

// Functions were a part of the language, and you could define a function that
// drew something, and then you could call that function to draw a something somewhere.

// I originally used it to render 2.5" laptop HDD development roadmaps to paste into
// Microsoft Office apps, as we were constantly updating them and we had a generic
// roadmap for the smaller customer, but for the major OEMs, we maintained a much-discussed
// and negotiated specific roadmap for that OEM.

// Sorry about the archaic code and what would today be dangerous or poor programming
// practice.  I wrote my own string class, hashtable class, etc.  I went through the
// code and converted to std::string, std::unordered_map, etc.

// I still use the app from time to time to make diagrams that are too hard to make in PowerPoint.
// The drawing illustrating zones and servo sectors in the "Disk Drive Concepts"
// material was made with the old app.

// Ivy just has the parser interpreter for the C language part of that old code.

// [1998 explanation follows]

//   Functions can be defined anywhere.
//
// - Functions are entered into the symbol table for
//   the code block that they were defined in.  That
//   means that same-name functions can be defined
//   in disjoint scopes and not be visible to each other
//   (e.g. "double f(){...}; ... { int f() {..}; }" is
//   OK.  The "int f()" is visible only in the nested block;
//
// - A forward declaration of a function without a function
//   body is entered into the symbol table as the equivalent
//   of an "unresolved external reference".  Therefore function
//   forward declarations must be fulfilled within the same scope.
//   (Of course, an unresolved forward reference can be
//   "hidden" by an actual definition of the same function
//   within a more deeply nested block.
//
// - Function names can be overloaded, meaning that a
//   function name is qualified with the type of each
//   of its arguments for lookup in the symbol table.
//   For example, "int f(int)" and "int f(double)" can
//   happily coexist.  When invoked, the one the caller
//   wants is distinguished by looking at the argument
//   "signature" or the number and type of each argument.
//   For functions with the same name, it is the
//   arguments that must be different; the return type
//   is not used to distinguish which function is
//   being referred to.  Thus you may not have
//   "int f(int)" and "double f(int)" within the same scope.
//
// - If a function is defined within a nested scope,
//   that is, defined within a nested {...} block, or
//   defined within the body of an enclosing function,
//   all of the variables in the containing scope are
//   visible to the enclosed function.  Any references
//   to such local variables of an enclosing scope
//   refer to the most recent version on the stack of
//   the enclosing scope.
//
//   The way we do this is that every block that has
//   variables defined has a symbol table.  Every code
//   block has a pointer to its symbol table, and a
//   pointer to the next higher level (containing) block.
//   Therefore, to perform symbol table lookups, we
//   lookup in this block's symbol table, and if not
//   found, we look in the symbol table of the next
//   higher level block, and so on.  The first level
//   block of a new function definition points to the
//   block that the function was contained in.  Each
//   block is part of a frame.  Frames are created
//   on the stack for each invocation of a function.
//   Each body points to its frame.  Each time a frame
//   is invoked, a push-down stack (actually, a linked
//   list) records the most recent frame offset location
//   on the stack.
//
//   So to summarize, a lookup for an identifier looks
//   up through the chain of nested blocks, checking
//   each block's symbol table until it the identifier
//   is found.  Then a "variable reference" is recorded
//   which consists of a pointer to the frame object,
//   and a pointer to the symbol table entry.
//
//   At run time, we add refer to the block's push-down
//   invocation stack to get the most recent location
//   of that type of frame on the stack, and then add
//   the "offset from beginning of frame" from the
//   symbol table entry to get the address of the
//   variable on the stack.



// Executable statements are queued in Blocks.

// Each block has a "code_list", for variable
// initialization statements and normal executable
// statements, and an "exit_list" containing statements
// to be run whenever exiting the block for any reason.
// Destructors for objects on the stack are found on
// the exit_list.

// During program parsing, there is a BlockStack of
// Blocks.  A new one is started for every nested
// level { ... }.  Each block will have its own
// SymbolTable if any variables are declared within
// that Block.  Variables are in scope from the point
// that they are declared until the end of the block.

// Variables declared within a particular block
// are also visible in any nested levels within
// that block, unless "hidden" by the declaration
// of a variable with the same name within the
// contained nested level.

// Variables declared as static are allocated on
// the stack in a permanent position below all stack
// frames associated with invocations of functions.

// After all code has been parsed, we have finished
// allocating at the bottom of the stack all variables
// found at the global level outside of any functions,
// as well as all variables declared as static in
// function bodies.  Then when we begin execution,
// the first stack frame associated with the invocation
// of a function is found immediately above all the
// static variables.

// Automatic variables are allocated on the stack
// at an offset from the beginning of the stack frame.
// The code list for the associated block contains
// the constructors and initiators for the automatic
// variables at some point before they are used, and
// the destructors are on the exit_list for each block.

// ??? what about initializers of static function
// variables?  What should we do?  A: run once!
// when first invoked - of course!  Stick 'em in
// the function code, but tag'em "run once".

// Execution starts with the global_Block.

	// Parsing: one Frame for every function definition
	//
	//    - Frame's first code Block goes on the BlockStack
	//      (chained to higher level enclosing scope code)
	//      to permit reference to variables within enclosing
	//      scope
	//
	// Program execution:
	//  "stack_offset" records start of most recent frame on stack

	// Contents placed on the stack during execution:
	//
	// 1) return value followed in sequence by function arguments
	//
	//    - If return value or arguments are objects of a type
	//      that requires construction/destruction, the caller
	//      constructs these objects and assigns the values of
	//      the arguments before invoking the function, and after
	//      the function's invoke() returns, the caller reads
	//      the return value and then destructs the return value
	//      and argument objects.
	//
	// 2) automatic variables allocated on stack by func. body
	//
	//    - Starting "next_available" offset within frame for
	//      the first Block (*p_code_Block) is the total size
	//      of the return value and argument objects.
	//
	//    - Automatic storage class variables on the stack
	//      are constructed by stmt_constructor statements
	//      embedded in the code_body and are destroyed by
	//      stmt_destructor statements in the destructors_code

// [end of 1998 explanation]

//    2015 notes
//    ----------
//
//    The Flex/Bison (formerly lex/yacc) parser is used to recognize the grammar
//    of a ivyscript programming language.
//
//    To build the code, you need to run Bison first against the ivy.y grammar definition,
//    in order to generate a token definition header file which is referred to in the lexer
//    (tokenizer) defined in ivy.l.
//
//    However, when the parser is running, the lexer (defined in ivy.l) generates tokens
//    fed into the grammar recognizer (defined in ivy.y), so the feed conceptually
//    is in the other direction.
//
//    The Bison C++ parser uses a user-provided object called a "context" that you
//    can refer to in your Bison code snippets.  The Bison grammar rules define various things
//    like expressions, statements, variable declarations, function definitions, etc.
//
//    The context is an Ivy_pgm object.
//
//    In the rule that recognizes, say, what was recognized as a function call,
//    you specify in the Bison rules a C++ code snippet that runs as the function call
//    is parsed, and which builds in the Ivy_pgm context object whatever data structures will
//    be needed at run time referring to the already-built data structures for the elements
//    recognized earlier by the parser as lower-level components of the function call.
//
//    As the parser runs, it builds statements (Stmt) and queues them in the "code" and "destructors"
//    list in a Block.
//
//    As statements are parsed, for each Block, we keep track of "next available" which is
//    an offset from where the Block will start on the "stack".
//
//    As you declare variables within a Block, next available is incremented by the size of
//    the variable type.  Some variable types, such as string need a constructor/destructor.
//
//    For variables needing a constructor, we chain a constructor statement to the beginning
//    of the "code" list for the block.  As other statement types are recognized they are
//    chained to the end of the "code" list.  If the variable type needs a destructor,
//    we chain a destructor statement in the "destructors" list.
//
//    "In place" or placement new constructors are used at the designated stack location
//    at execution time when the block is entered, and then as the block is exited, we
//    run the destructor in place at the stack location.
//
//    Recognizing known variable or function names, including built-in functions
//    is handled using a SymbolTable in each Block.
//
//    The symbol table in a block is built as statements are parsed, and so
//    if we are parsing an expression and we see a variable reference, we look
//    up the variable name in the symbol table and if we don't find it there,
//    we look in the symbol table in successively higher level enclosing blocks
//    and then for function calls, we then also look in the table of built in
//    functions.
//

