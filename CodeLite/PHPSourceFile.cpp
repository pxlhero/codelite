#include "PHPSourceFile.h"
#include <wx/ffile.h>
#include "PHPScannerTokens.h"
#include "PHPEntityNamespace.h"
#include "PHPEntityFunction.h"
#include "PHPEntityVariable.h"
#include <wx/arrstr.h>
#include "PHPEntityClass.h"
#include "PHPDocVisitor.h"

#define NEXT_TOKEN_BREAK_IF_NOT(t, action) \
    {                                      \
        if(!NextToken(token)) break;       \
        if(token.type != t) {              \
            action;                        \
            break;                         \
        }                                  \
    }

PHPSourceFile::PHPSourceFile(const wxString& content)
    : m_text(content)
    , m_parseFunctionBody(false)
    , m_depth(0)
    , m_reachedEOF(false)
{
    m_scanner = ::phpLexerNew(content, kPhpLexerOpt_ReturnComments);
}

PHPSourceFile::PHPSourceFile(const wxFileName& filename)
    : m_filename(filename)
    , m_parseFunctionBody(false)
    , m_depth(0)
    , m_reachedEOF(false)
{
    // Filename is kept in absolute path
    m_filename.MakeAbsolute();

    wxString content;
    wxFFile fp(filename.GetFullPath(), "rb");
    if(fp.IsOpened()) {
        fp.ReadAll(&content, wxConvISO8859_1);
        fp.Close();
    }
    m_text.swap(content);
    m_scanner = ::phpLexerNew(m_text, kPhpLexerOpt_ReturnComments);
}

PHPSourceFile::~PHPSourceFile()
{
    if(m_scanner) {
        ::phpLexerDestroy(&m_scanner);
    }
}

void PHPSourceFile::Parse(int exitDepth)
{
    int retDepth = exitDepth;
    phpLexerToken token;
    while(NextToken(token)) {
        switch(token.type) {
        case '=':
            m_lookBackTokens.clear();
            break;
        case '{':
            m_lookBackTokens.clear();
            break;
        case '}':
            m_lookBackTokens.clear();
            if(m_depth == retDepth) {
                return;
            }
            break;
        case ';':
            m_lookBackTokens.clear();
            break;
        case kPHP_T_VARIABLE:
            if(!CurrentScope()->Is(kEntityTypeClass)) {
                // A global variable
                OnVariable(token);
            }
            break;

        case kPHP_T_PUBLIC:
        case kPHP_T_PRIVATE:
        case kPHP_T_PROTECTED: {
            int visibility = token.type;
            PHPEntityClass* cls = CurrentScope()->Cast<PHPEntityClass>();
            if(cls) {
                /// keep the current token
                m_lookBackTokens.push_back(token);

                // Now we have a small problem here:
                // public can be a start for a member or a function
                // we let the lexer run forward until it finds kPHP_T_VARIABLE (for variable)
                // or kPHP_T_IDENTIFIER
                int what = ReadUntilFoundOneOf(kPHP_T_VARIABLE, kPHP_T_FUNCTION, token);
                if(what == kPHP_T_VARIABLE) {
                    // A variable
                    PHPEntityBase::Ptr_t member(new PHPEntityVariable());
                    member->SetFilename(m_filename.GetFullPath());
                    PHPEntityVariable* var = member->Cast<PHPEntityVariable>();
                    var->SetVisibility(visibility);
                    var->SetFullName(token.text);
                    size_t flags = LookBackForVariablesFlags();
                    var->SetFlag(kVar_Member);
                    var->SetFlag(kVar_Const, flags & kVar_Const);
                    var->SetFlag(kVar_Static, flags & kVar_Static);
                    var->SetLine(token.lineNumber);
                    CurrentScope()->AddChild(member);
                    if(!ConsumeUntil(';')) return;
                } else if(what == kPHP_T_FUNCTION) {
                    // A function...
                    OnFunction();
                    m_lookBackTokens.clear();
                }
            }
            break;
        }
        case kPHP_T_DEFINE:
            // Define statement
            OnDefine(token);
            break;
        case kPHP_T_CONST:
            if(ReadUntilFound(kPHP_T_IDENTIFIER, token)) {
                // constant
                PHPEntityBase::Ptr_t member(new PHPEntityVariable());
                member->SetFilename(m_filename.GetFullPath());
                PHPEntityVariable* var = member->Cast<PHPEntityVariable>();
                var->SetFullName(token.text);
                var->SetLine(token.lineNumber);
                var->SetFlag(kVar_Member);
                var->SetFlag(kVar_Const);
                CurrentScope()->AddChild(member);
                if(!ConsumeUntil(';')) return;
            }
            break;
        case kPHP_T_REQUIRE:
        case kPHP_T_REQUIRE_ONCE:
        case kPHP_T_INCLUDE:
        case kPHP_T_INCLUDE_ONCE:
            // Handle include files
            m_lookBackTokens.clear();
            break;
        case kPHP_T_USE:
            // Found outer 'use' statement - construct the alias table
            if(Class()) {
                // inside a class, this means that this is a 'use <trait>;'
                OnUseTrait();
            } else {
                // alias table
                OnUse();
            }
            m_lookBackTokens.clear();
            break;
        case kPHP_T_CLASS:
        case kPHP_T_INTERFACE:
        case kPHP_T_TRAIT:
            // Found class
            OnClass(token);
            m_lookBackTokens.clear();
            break;
        case kPHP_T_NAMESPACE:
            // Found a namespace
            OnNamespace();
            m_lookBackTokens.clear();
            break;
        case kPHP_T_FUNCTION:
            // Found function
            OnFunction();
            m_lookBackTokens.clear();
            break;
        default:
            // Keep the token
            break;
        }
    }
    PhaseTwo();
}

void PHPSourceFile::OnUse()
{
    wxString fullname, alias, temp;
    phpLexerToken token;
    bool cont = true;
    while(cont && NextToken(token)) {
        switch(token.type) {
        case ',':
        case ';': {
            if(fullname.IsEmpty()) {
                // no full name yet
                fullname.swap(temp);

            } else if(alias.IsEmpty()) {
                alias.swap(temp);
            }

            if(alias.IsEmpty()) {
                // no alias provided, use the last part of the fullname
                alias = fullname.AfterLast('\\');
            }

            if(!fullname.IsEmpty() && !alias.IsEmpty()) {
                // Use namespace is alway refered as fullpath namespace
                // So writing:
                // use Zend\Mvc\Controll\Action;
                // is equal for writing:
                // use \Zend\Mvc\Controll\Action;
                // For simplicitiy, we change it to fully qualified path
                // so parsing is easier
                if(!fullname.StartsWith("\\")) {
                    fullname.Prepend("\\");
                }
                m_aliases.insert(std::make_pair(alias, MakeIdentifierAbsolute(fullname)));
            }
            temp.clear();
            fullname.clear();
            alias.clear();
            if(token.type == ';') {
                cont = false;
            }
        } break;
        case kPHP_T_AS: {
            fullname.swap(temp);
            temp.clear();
        } break;
        default:
            temp << token.text;
            break;
        }
    }
}

void PHPSourceFile::OnNamespace()
{
    // Read until we find the line delimiter ';' or EOF found
    wxString path;
    phpLexerToken token;
    while(NextToken(token)) {
        if(token.type == ';') {
            break;
        }

        // Make sure that the namespace path is alway set in absolute path
        // i.e. starts with kPHP_T_NS_SEPARATOR
        if(path.IsEmpty() && token.type != kPHP_T_NS_SEPARATOR) {
            path << "\\";
        }
        path << token.text;
    }

    if(m_scopes.empty()) {
        // no scope is set, push the global scope
        m_scopes.push_back(PHPEntityBase::Ptr_t(new PHPEntityNamespace()));
        PHPEntityNamespace* ns = CurrentScope()->Cast<PHPEntityNamespace>();
        if(ns) {
            ns->SetFullName(path); // Global namespace
        }
    } else {
        // PHP parsing error... (namespace must be the first thing on the file)
    }
}

void PHPSourceFile::OnFunction()
{
    // read the next token
    phpLexerToken token;
    if(!NextToken(token)) {
        return;
    }

    PHPEntityFunction* func(NULL);
    int funcDepth(0);
    if(token.type == kPHP_T_IDENTIFIER) {
        // the function name
        func = new PHPEntityFunction();
        func->SetFullName(token.text);
        func->SetLine(token.lineNumber);

    } else if(token.type == '(') {
        funcDepth = 1; // Since we already consumed the open brace
        // anonymous function
        func = new PHPEntityFunction();
        func->SetLine(token.lineNumber);
    }

    if(!func) return;
    PHPEntityBase::Ptr_t funcPtr(func);

    // add the function to the current scope
    CurrentScope()->AddChild(funcPtr);

    // Set the function as the current scope
    m_scopes.push_back(funcPtr);

    // update function attributes
    ParseFunctionSignature(funcDepth);
    func->SetFlags(LookBackForFunctionFlags());
    if(LookBackTokensContains(kPHP_T_ABSTRACT) || // The 'abstract modifier was found for this this function
       (funcPtr->Parent() && funcPtr->Parent()->Is(kEntityTypeClass) &&
        funcPtr->Parent()->Cast<PHPEntityClass>()->IsInterface())) // We are inside an interface
    {
        // Mark this function as an abstract function
        func->SetFlags(func->GetFlags() | kFunc_Abstract);
    }

    if(func->HasFlag(kFunc_Abstract)) {
        // an abstract function - it has no body
        if(!ConsumeUntil(';')) {
            // could not locate the function delimiter, remove it from the stack
            // we probably reached EOF here
            m_scopes.pop_back();
        }

    } else {

        if(ReadUntilFound('{', token)) {
            // found the function body starting point
            if(IsParseFunctionBody()) {
                ParseFunctionBody();
            } else {
                // Consume the function body
                ConsumeFunctionBody();
            }
        } else {
            // could not locate the open brace!
            // remove this function from the stack
            m_scopes.pop_back();
        }
    }

    // Remove the current function from the scope list
    if(!m_reachedEOF) {
        m_scopes.pop_back();
    }
    m_lookBackTokens.clear();
}

PHPEntityBase::Ptr_t PHPSourceFile::CurrentScope()
{
    if(m_scopes.empty()) {
        // no scope is set, push the global scope
        m_scopes.push_back(PHPEntityBase::Ptr_t(new PHPEntityNamespace()));
        CurrentScope()->SetFullName("\\"); // Global namespace
    }
    return m_scopes.back();
}

size_t PHPSourceFile::LookBackForFunctionFlags()
{
    size_t flags(0);
    for(size_t i = 0; i < m_lookBackTokens.size(); ++i) {
        const phpLexerToken& tok = m_lookBackTokens.at(i);
        if(tok.type == kPHP_T_ABSTRACT) {
            flags |= kFunc_Abstract;

        } else if(tok.type == kPHP_T_FINAL) {
            flags |= kFunc_Final;
        } else if(tok.type == kPHP_T_STATIC) {
            flags |= kFunc_Static;

        } else if(tok.type == kPHP_T_PUBLIC) {
            flags |= kFunc_Public;
            flags &= ~kFunc_Private;
            flags &= ~kFunc_Protected;

        } else if(tok.type == kPHP_T_PRIVATE) {
            flags |= kFunc_Private;
            flags &= ~kFunc_Public;
            flags &= ~kFunc_Protected;

        } else if(tok.type == kPHP_T_PROTECTED) {
            flags |= kFunc_Protected;
            flags &= ~kFunc_Public;
            flags &= ~kFunc_Private;
        }
    }
    return flags;
}

void PHPSourceFile::ParseFunctionSignature(int startingDepth)
{
    phpLexerToken token;
    if(startingDepth == 0) {
        // loop until we find the open brace
        while(NextToken(token)) {
            if(token.type == '(') {
                ++startingDepth;
                break;
            }
        }
        if(startingDepth == 0) return;
    }

    // at this point the 'depth' is 1
    int depth = 1;
    wxString typeHint;
    wxString defaultValue;
    PHPEntityVariable* var(NULL);
    bool collectingDefaultValue = false;
    while(NextToken(token)) {
        switch(token.type) {
        case kPHP_T_VARIABLE:
            var = new PHPEntityVariable();
            var->SetFullName(token.text);
            var->SetLine(token.lineNumber);
            var->SetFilename(m_filename);
            // Mark this variable as function argument
            var->SetFlag(kVar_FunctionArg);
            if(typeHint.EndsWith("&")) {
                var->SetIsReference(true);
                typeHint.RemoveLast();
            }
            var->SetTypeHint(MakeIdentifierAbsolute(typeHint));
            break;
        case '(':
            depth++;
            if(collectingDefaultValue) {
                defaultValue << "(";
            }
            break;
        case ')':
            depth--;
            // if the depth goes under 1 - we are done
            if(depth < 1) {
                if(var) {
                    var->SetDefaultValue(defaultValue);
                    CurrentScope()->AddChild(PHPEntityBase::Ptr_t(var));
                }
                return;

            } else if(depth) {
                defaultValue << token.text;
            }
            break;
        case '=':
            // default value
            collectingDefaultValue = true;
            break;
        case ',':
            if(var) {
                var->SetDefaultValue(defaultValue);
                CurrentScope()->AddChild(PHPEntityBase::Ptr_t(var));
            }
            var = NULL;
            typeHint.Clear();
            defaultValue.Clear();
            collectingDefaultValue = false;
            break;
        default:
            if(collectingDefaultValue) {
                defaultValue << token.text;
            } else {
                typeHint << token.text;
            }
            break;
        }
    }
}

void PHPSourceFile::PrintStdout()
{
    // print the alias table
    wxPrintf("Alias table:\n");
    wxPrintf("===========\n");
    std::map<wxString, wxString>::iterator iter = m_aliases.begin();
    for(; iter != m_aliases.end(); ++iter) {
        wxPrintf("%s => %s\n", iter->first, iter->second);
    }
    wxPrintf("===========\n");
    if(m_scopes.empty()) return;
    m_scopes.front()->PrintStdout(0);
}

bool PHPSourceFile::ReadUntilFound(int delim, phpLexerToken& token)
{
    // loop until we find the open brace
    while(NextToken(token)) {
        if(token.type == delim) {
            return true;
        }
    }
    return false;
}

void PHPSourceFile::ConsumeFunctionBody()
{
    int depth = m_depth;
    phpLexerToken token;
    while(NextToken(token)) {
        switch(token.type) {
        case '}':
            if(m_depth < depth) {
                return;
            }
            break;
        default:
            break;
        }
    }
}

void PHPSourceFile::ParseFunctionBody()
{
    m_lookBackTokens.clear();

    // when we reach the current depth-1 -> leave
    int exitDepth = m_depth - 1;
    phpLexerToken token;
    PHPEntityBase::Ptr_t var(NULL);
    while(NextToken(token)) {
        switch(token.type) {
        case '{':
            m_lookBackTokens.clear();
            break;
        case '}':
            m_lookBackTokens.clear();
            if(m_depth == exitDepth) {
                return;
            }
            break;
        case ';':
            m_lookBackTokens.clear();
            break;
        case kPHP_T_VARIABLE: {
            var.Reset(new PHPEntityVariable());
            var->SetFullName(token.text);
            var->SetFilename(m_filename.GetFullPath());
            var->SetLine(token.lineNumber);
            CurrentScope()->AddChild(var);

            // Peek at the next token
            if(!NextToken(token)) return; // EOF
            if(token.type != '=') {
                m_lookBackTokens.clear();
                var.Reset(NULL);
                UngetToken(token);

            } else {

                wxString expr;
                if(!ReadExpression(expr)) return; // EOF

                // Optimize 'new ClassName(..)' expression
                if(expr.StartsWith("new")) {
                    expr = expr.Mid(3);
                    expr.Trim().Trim(false);
                    expr = expr.BeforeFirst('(');
                    expr.Trim().Trim(false);
                    var->Cast<PHPEntityVariable>()->SetTypeHint(MakeIdentifierAbsolute(expr));

                } else {
                    // keep the expression
                    var->Cast<PHPEntityVariable>()->SetExpressionHint(expr);
                }
            }
        } break;
        default:
            break;
        }
    }
}

wxString PHPSourceFile::ReadType()
{
    bool cont = true;
    wxString type;
    phpLexerToken token;
    while(cont && NextToken(token)) {
        switch(token.type) {
        case kPHP_T_IDENTIFIER:
            type << token.text;
            break;

        case kPHP_T_NS_SEPARATOR:
            type << token.text;
            break;

        // special cases that must always be handled
        case '{':
            cont = false;
            break;
        // end of special cases
        default:
            cont = false;
            break;
        }
    }
    type = MakeIdentifierAbsolute(type);
    return type;
}

PHPEntityBase::Ptr_t PHPSourceFile::Namespace()
{
    if(m_scopes.empty()) {
        return CurrentScope();
    }
    return *m_scopes.begin();
}

wxString PHPSourceFile::LookBackForTypeHint()
{
    if(m_lookBackTokens.empty()) return wxEmptyString;
    wxArrayString tokens;

    for(int i = (int)m_lookBackTokens.size() - 1; i >= 0; --i) {
        if(m_lookBackTokens.at(i).type == kPHP_T_IDENTIFIER || m_lookBackTokens.at(i).type == kPHP_T_NS_SEPARATOR) {
            tokens.Insert(m_lookBackTokens.at(i).text, 0);
        } else {
            break;
        }
    }

    wxString type;
    for(size_t i = 0; i < tokens.GetCount(); ++i) {
        type << tokens.Item(i);
    }
    return type;
}

void PHPSourceFile::PhaseTwo()
{
    // Visit each entity found during the parsing stage
    // and try to match it with its phpdoc comment block (we do this by line number)
    // the visitor also makes sure that each entity is properly assigned with the current file name
    PHPDocVisitor visitor(*this, m_comments);
    visitor.Visit(Namespace());
}

bool PHPSourceFile::NextToken(phpLexerToken& token)
{
    bool res = ::phpLexerNext(m_scanner, token);
    if(res && token.type == kPHP_T_C_COMMENT) {
        m_comments.push_back(token);
    } else if(token.type == '{') {
        m_depth++;
    } else if(token.type == '}') {
        m_depth--;
    } else if(token.type == ';') {
        m_lookBackTokens.clear();
    }
    if(!res) m_reachedEOF = true;
    if(res) m_lookBackTokens.push_back(token);
    return res;
}

wxString PHPSourceFile::MakeIdentifierAbsolute(const wxString& type)
{
    wxString typeWithNS(type);
    typeWithNS.Trim().Trim(false);

    if(typeWithNS == "string" || typeWithNS == "array" || typeWithNS == "mixed" || typeWithNS == "bool" ||
       typeWithNS == "int" || typeWithNS == "integer" || typeWithNS == "boolean" || typeWithNS == "double") {
        // primitives, don't bother...
        return typeWithNS;
    }

    if(typeWithNS.IsEmpty()) return "";
    // If the symbol contains namespace separator
    // Convert it full path and return (prepend namespace separator)
    if(typeWithNS.Contains("\\")) {
        if(!typeWithNS.StartsWith("\\")) {
            typeWithNS.Prepend("\\");
        }
        return typeWithNS;
    }

    if(typeWithNS.StartsWith("\\")) {
        return typeWithNS;
    }

    // Use the alias table first
    if(m_aliases.find(type) != m_aliases.end()) {
        return m_aliases.find(type)->second;
    }

    wxString ns = Namespace()->GetFullName();
    if(!ns.EndsWith("\\")) {
        ns << "\\";
    }

    typeWithNS.Prepend(ns);
    return typeWithNS;
}

void PHPSourceFile::OnClass(const phpLexerToken& tok)
{
    // A "complex" example: class A extends BaseClass implements C, D {}

    // Read until we get the class name
    phpLexerToken token;
    while(NextToken(token)) {
        if(token.IsAnyComment()) continue;
        if(token.type != kPHP_T_IDENTIFIER) {
            // expecting the class name
            return;
        }
        break;
    }

    // create new class entity
    PHPEntityBase::Ptr_t klass(new PHPEntityClass());
    klass->SetFilename(m_filename.GetFullPath());
    PHPEntityClass* pClass = klass->Cast<PHPEntityClass>();
    // Is the class an interface?
    pClass->SetIsInterface(tok.type == kPHP_T_INTERFACE);
    pClass->SetIsTrait(tok.type == kPHP_T_TRAIT);
    pClass->SetFullName(MakeIdentifierAbsolute(token.text));
    pClass->SetLine(token.lineNumber);

    while(NextToken(token)) {
        if(token.IsAnyComment()) continue;
        switch(token.type) {
        case kPHP_T_EXTENDS: {
            // inheritance
            if(!ReadUntilFound(kPHP_T_IDENTIFIER, token)) return;
            pClass->SetExtends(MakeIdentifierAbsolute(token.text));
        } break;
        case kPHP_T_IMPLEMENTS: {
            wxArrayString implements;
            if(!ReadCommaSeparatedIdentifiers('{', implements)) return;
            pClass->SetImplements(implements);

        } break;
        case '{': {
            // entering the class body
            // add the current class to the current scope
            CurrentScope()->AddChild(klass);
            m_scopes.push_back(klass);
            Parse(m_depth - 1);
            if(!m_reachedEOF) {
                m_scopes.pop_back();
            }
            return;
        }
        default:
            break;
        }
    }
}

bool PHPSourceFile::ReadCommaSeparatedIdentifiers(int delim, wxArrayString& list)
{
    phpLexerToken token;
    wxString temp;
    while(NextToken(token)) {
        if(token.IsAnyComment()) continue;
        if(token.type == delim) {
            if(!temp.IsEmpty() && list.Index(temp) == wxNOT_FOUND) {
                list.Add(MakeIdentifierAbsolute(temp));
            }
            UngetToken(token);
            return true;
        }

        switch(token.type) {
        case ',':
            if(list.Index(temp) == wxNOT_FOUND) {
                list.Add(MakeIdentifierAbsolute(temp));
            }
            temp.clear();
            break;
        default:
            temp << token.text;
            break;
        }
    }
    return false;
}

bool PHPSourceFile::ConsumeUntil(int delim)
{
    phpLexerToken token;
    while(NextToken(token)) {
        if(token.type == delim) {
            return true;
        }
    }
    return false;
}

bool PHPSourceFile::ReadExpression(wxString& expression)
{
    expression.clear();
    phpLexerToken token;
    int depth(0);
    while(NextToken(token)) {
        if(token.type == ';') {
            return true;

        } else if(token.type == '{') {
            UngetToken(token);
            return true;
        }

        switch(token.type) {
        case kPHP_T_REQUIRE:
        case kPHP_T_REQUIRE_ONCE:
            expression.clear();
            return false;
        
        case kPHP_T_STRING_CAST:
        case kPHP_T_CONSTANT_ENCAPSED_STRING:
        case kPHP_T_C_COMMENT:
        case kPHP_T_CXX_COMMENT:
            // skip comments and strings
            break;
        case '(':
            depth++;
            expression << "(";
            break;
        case ')':
            depth--;
            if(depth == 0) {
                expression << ")";
            }
            break;
        case kPHP_T_NEW:
            if(depth == 0) {
                expression << token.text << " ";
            }
            break;
        default:
            if(depth == 0) {
                expression << token.text;
            }
            break;
        }
    }
    // reached EOF
    return false;
}

void PHPSourceFile::UngetToken(const phpLexerToken& token)
{
    ::phpLexerUnget(m_scanner);
    // undo any depth / comments
    if(token.type == '{') {
        m_depth--;
    } else if(token.type == '}') {
        m_depth++;
    } else if(token.type == kPHP_T_C_COMMENT && !m_comments.empty()) {
        m_comments.erase(m_comments.begin() + m_comments.size() - 1);
    }
}

const PHPEntityBase* PHPSourceFile::Class()
{
    PHPEntityBase::Ptr_t curScope = CurrentScope();
    PHPEntityBase* pScope = curScope.Get();
    while(pScope) {
        PHPEntityClass* cls = pScope->Cast<PHPEntityClass>();
        if(cls) {
            // this scope is a class
            return pScope;
        }
        pScope = pScope->Parent();
    }
    return NULL;
}

int PHPSourceFile::ReadUntilFoundOneOf(int delim1, int delim2, phpLexerToken& token)
{
    // loop until we find the open brace
    while(NextToken(token)) {
        if(token.type == delim1) {
            return delim1;
        } else if(token.type == delim2) {
            return delim2;
        }
    }
    return wxNOT_FOUND;
}

bool PHPSourceFile::LookBackTokensContains(int type) const
{
    for(size_t i = 0; i < m_lookBackTokens.size(); ++i) {
        if(m_lookBackTokens.at(i).type == type) return true;
    }
    return false;
}

size_t PHPSourceFile::LookBackForVariablesFlags()
{
    size_t flags(kVar_Public);
    for(size_t i = 0; i < m_lookBackTokens.size(); ++i) {
        const phpLexerToken& tok = m_lookBackTokens.at(i);
        if(tok.type == kPHP_T_STATIC) {
            flags |= kVar_Static;

        } else if(tok.type == kPHP_T_CONST) {
            flags |= kVar_Const;

        } else if(tok.type == kPHP_T_PUBLIC) {
            flags |= kVar_Public;
            flags &= ~kVar_Private;
            flags &= ~kVar_Protected;

        } else if(tok.type == kPHP_T_PRIVATE) {
            flags |= kVar_Private;
            flags &= ~kVar_Public;
            flags &= ~kVar_Protected;

        } else if(tok.type == kPHP_T_PROTECTED) {
            flags |= kVar_Protected;
            flags &= ~kVar_Private;
            flags &= ~kVar_Public;
        }
    }
    return flags;
}

void PHPSourceFile::OnVariable(const phpLexerToken& tok)
{
    phpLexerToken token;
    // Read until we find the ';'
    std::vector<phpLexerToken> tokens;
    PHPEntityBase::Ptr_t var(new PHPEntityVariable());
    var->SetFullName(tok.text);
    var->SetFilename(m_filename.GetFullPath());
    var->SetLine(tok.lineNumber);
    if(!CurrentScope()->FindChild(var->GetFullName(), true)) {
        CurrentScope()->AddChild(var);
    }

    if(!NextToken(token)) return;

    if(token.type != '=') {
        m_lookBackTokens.clear();
        return;
    }

    wxString expr;
    if(!ReadExpression(expr)) return; // EOF

    // Optimize 'new ClassName(..)' expression
    if(expr.StartsWith("new")) {
        expr = expr.Mid(3);
        expr.Trim().Trim(false);
        expr = expr.BeforeFirst('(');
        expr.Trim().Trim(false);
        var->Cast<PHPEntityVariable>()->SetTypeHint(MakeIdentifierAbsolute(expr));

    } else {
        // keep the expression
        var->Cast<PHPEntityVariable>()->SetExpressionHint(expr);
    }
}

PHPEntityBase::List_t PHPSourceFile::GetAliases() const
{
    PHPEntityBase::List_t aliases;
    std::map<wxString, wxString>::const_iterator iter = m_aliases.begin();
    for(; iter != m_aliases.end(); ++iter) {
        // wrap each alias with class entity
        PHPEntityBase::Ptr_t klass(new PHPEntityClass());
        klass->SetFullName(iter->second);
        klass->SetShortName(iter->first);
        klass->SetFilename(GetFilename());
        aliases.push_back(klass);
    }
    return aliases;
}

void PHPSourceFile::OnDefine(const phpLexerToken& tok)
{
    phpLexerToken token;
    if(!NextToken(token)) return; // EOF
    if(token.type != '(') {
        ConsumeUntil(';');
        return;
    }
    if(!NextToken(token)) return; // EOF
    if(token.type != kPHP_T_CONSTANT_ENCAPSED_STRING) {
        ConsumeUntil(';');
        return;
    }
    // Remove the quotes
    wxString varName = token.text;
    if((varName.StartsWith("\"") && varName.EndsWith("\"")) || (varName.StartsWith("'") && varName.EndsWith("'"))) {
        varName.Remove(0, 1);
        varName.RemoveLast();
        // define() defines constants exactly as it was instructed
        // i.e. it does not take the current namespace into consideration
        PHPEntityBase::Ptr_t var(new PHPEntityVariable());

        // Convert the variable into fullpath + relative name
        if(!varName.StartsWith("\\")) {
            varName.Prepend("\\");
        }
        wxString shortName = varName.AfterLast('\\');
        var->SetFullName(varName);
        var->SetShortName(shortName);
        var->SetFlag(kVar_Define);
        var->SetFilename(GetFilename());
        var->SetLine(tok.lineNumber);

        // We keep the defines in a special list
        // this is because 'define' does not obay to the current scope
        m_defines.push_back(var);
    }
    // Always consume the 'define' statement
    ConsumeUntil(';');
}

void PHPSourceFile::OnUseTrait()
{
    PHPEntityBase::Ptr_t clas = CurrentScope();
    if(!clas) return;
    
    // Collect the identifiers followed the 'use' statement
    wxArrayString identifiers;
    wxString tempname;
    phpLexerToken token;
    while(NextToken(token)) {
        switch(token.type) {
        case ',': {
            if(!tempname.IsEmpty()) {
                identifiers.Add(MakeIdentifierAbsolute(tempname));
            }
            tempname.clear();
        }
        break;
        case ';': {
            if(!tempname.IsEmpty()) {
                identifiers.Add(MakeIdentifierAbsolute(tempname));
            }
            tempname.clear();
            
            // add the traits as list of 'extends'
            clas->Cast<PHPEntityClass>()->SetTraits(identifiers);
            return;
        } break;
        default:
            tempname << token.text;
            break;
        }
    }
}
