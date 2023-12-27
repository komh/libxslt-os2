/*
xmlParserInputPtr xmlNoNetExternalEntityLoader(const char *URL,
	                                       const char *ID,
					       xmlParserCtxtPtr ctxt);

 * types.c: converter functions between the internal representation
 *          and the Python objects
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */
#include "libxml_wrap.h"
#include <libxml/xpathInternals.h>

#if PY_MAJOR_VERSION >= 3
#define PY_IMPORT_STRING_SIZE PyUnicode_FromStringAndSize
#define PY_IMPORT_STRING PyUnicode_FromString
#define PY_IMPORT_INT PyLong_FromLong
#else
#define PY_IMPORT_STRING_SIZE PyString_FromStringAndSize
#define PY_IMPORT_STRING PyString_FromString
#define PY_IMPORT_INT PyInt_FromLong
#endif

#if PY_MAJOR_VERSION >= 3
#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32

#ifndef XML_IGNORE_FPTR_CAST_WARNINGS
#define XML_IGNORE_FPTR_CAST_WARNINGS
#endif
#ifndef XML_POP_WARNINGS
#define XML_POP_WARNINGS
#endif

#include <windows.h>
#include <crtdbg.h>

/* Taken from info on MSDN site, as we may not have the Windows WDK/DDK headers */
typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID    Pointer;
  } DUMMYUNIONNAME;
  ULONG_PTR Information;
} IO_STATUS_BLOCK;

typedef struct _FILE_ACCESS_INFORMATION {
  ACCESS_MASK AccessFlags;
} FILE_ACCESS_INFORMATION;

typedef NTSTATUS (*t_NtQueryInformationFile) (HANDLE           FileHandle,
                                              IO_STATUS_BLOCK *IoStatusBlock,
                                              PVOID            FileInformation,
                                              ULONG            Length,
                                              int              FileInformationClass); /* this is an Enum */

#if (defined (_MSC_VER) && _MSC_VER >= 1400)
/*
 * This is the (empty) invalid parameter handler
 * that is used for Visual C++ 2005 (and later) builds
 * so that we can use this instead of the system automatically
 * aborting the process.
 *
 * This is necessary as we use _get_oshandle() to check the validity
 * of the file descriptors as we close them, so when an invalid file
 * descriptor is passed into that function as we check on it, we get
 * -1 as the result, instead of the gspawn helper program aborting.
 *
 * Please see http://msdn.microsoft.com/zh-tw/library/ks2530z6%28v=vs.80%29.aspx
 * for an explanation on this.
 */
void
myInvalidParameterHandler(const wchar_t *expression,
                          const wchar_t *function,
                          const wchar_t *file,
                          unsigned int   line,
                          uintptr_t      pReserved)
{
}
#endif
#else
#include <unistd.h>
#include <fcntl.h>
#endif

FILE *
libxml_PyFileGet(PyObject *f) {
    FILE *res;
    const char *mode;
    int fd = PyObject_AsFileDescriptor(f);

#ifdef _WIN32
    intptr_t w_fh = -1;
    HMODULE hntdll = NULL;
    IO_STATUS_BLOCK status_block;
    FILE_ACCESS_INFORMATION ai;
    t_NtQueryInformationFile NtQueryInformationFile;
    BOOL is_read = FALSE;
    BOOL is_write = FALSE;
    BOOL is_append = FALSE;

#if (defined (_MSC_VER) && _MSC_VER >= 1400)
    /* set up our empty invalid parameter handler */
    _invalid_parameter_handler oldHandler, newHandler;
    newHandler = myInvalidParameterHandler;
    oldHandler = _set_invalid_parameter_handler(newHandler);

    /* Disable the message box for assertions. */
    _CrtSetReportMode(_CRT_ASSERT, 0);
#endif

    w_fh = _get_osfhandle(fd);

    if (w_fh == -1)
        return(NULL);

    hntdll = GetModuleHandleW(L"ntdll.dll");

    if (hntdll == NULL)
        return(NULL);
XML_IGNORE_FPTR_CAST_WARNINGS
    NtQueryInformationFile = (t_NtQueryInformationFile)GetProcAddress(hntdll, "NtQueryInformationFile");
XML_POP_WARNINGS

    if (NtQueryInformationFile != NULL &&
        (NtQueryInformationFile((HANDLE)w_fh,
                               &status_block,
                               &ai,
                                sizeof(FILE_ACCESS_INFORMATION),
                                8) == 0)) /* 8 means "FileAccessInformation" */
        {
            if (ai.AccessFlags & FILE_READ_DATA)
                is_read = TRUE;
            if (ai.AccessFlags & FILE_WRITE_DATA)
                is_write = TRUE;
            if (ai.AccessFlags & FILE_APPEND_DATA)
                is_append = TRUE;

            if (is_write) {
                if (is_read) {
                    if (is_append)
                        mode = "a+";
                    else
                        mode = "rw";
                } else {
                    if (is_append)
                        mode = "a";
                    else
                        mode = "w";
                }
            } else {
                if (is_append)
                    mode = "r+";
                else
                    mode = "r";
            }
        }

    FreeLibrary(hntdll);

    if (!is_write && !is_read) /* also happens if we did not load or run NtQueryInformationFile() successfully */
        return(NULL);
#else
    int flags;

    /*
     * macOS returns O_RDWR for standard streams, but fails to write to
     * stdout or stderr when opened with fdopen(dup_fd, "rw").
     */
    switch (fd) {
        case STDIN_FILENO:
            mode = "r";
            break;
        case STDOUT_FILENO:
        case STDERR_FILENO:
            mode = "w";
            break;
        default:
            /*
             * Get the flags on the fd to understand how it was opened
             */
            flags = fcntl(fd, F_GETFL, 0);
            switch (flags & O_ACCMODE) {
                case O_RDWR:
                    if (flags & O_APPEND)
                        mode = "a+";
                    else
                        mode = "rw";
                    break;
                case O_RDONLY:
                    if (flags & O_APPEND)
                        mode = "r+";
                    else
                        mode = "r";
                    break;
                case O_WRONLY:
                    if (flags & O_APPEND)
                        mode = "a";
                    else
                        mode = "w";
                    break;
                default:
                    return(NULL);
            }
    }
#endif

    /*
     * the FILE struct gets a new fd, so that it can be closed
     * independently of the file descriptor given. The risk though is
     * lack of sync. So at the python level sync must be implemented
     * before and after a conversion took place. No way around it
     * in the Python3 infrastructure !
     * The duplicated fd and FILE * will be released in the subsequent
     * call to libxml_PyFileRelease() which must be generated accordingly
     */
    fd = dup(fd);
    if (fd == -1)
        return(NULL);
    res = fdopen(fd, mode);
    if (res == NULL) {
        close(fd);
	return(NULL);
    }
    return(res);
}

void libxml_PyFileRelease(FILE *f) {
    if (f != NULL)
        fclose(f);
}
#endif

PyObject *
libxml_intWrap(int val)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_intWrap: val = %d\n", val);
#endif
    ret = PY_IMPORT_INT((long) val);
    return (ret);
}

PyObject *
libxml_longWrap(long val)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_longWrap: val = %ld\n", val);
#endif
    ret = PY_IMPORT_INT(val);
    return (ret);
}

PyObject *
libxml_doubleWrap(double val)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_doubleWrap: val = %f\n", val);
#endif
    ret = PyFloat_FromDouble((double) val);
    return (ret);
}

PyObject *
libxml_charPtrWrap(char *str)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlcharPtrWrap: str = %s\n", str);
#endif
    if (str == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    /* TODO: look at deallocation */
    ret = PY_IMPORT_STRING(str);
    xmlFree(str);
    return (ret);
}

PyObject *
libxml_charPtrConstWrap(const char *str)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlcharPtrWrap: str = %s\n", str);
#endif
    if (str == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    /* TODO: look at deallocation */
    ret = PY_IMPORT_STRING(str);
    return (ret);
}

PyObject *
libxml_xmlCharPtrWrap(xmlChar * str)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlCharPtrWrap: str = %s\n", str);
#endif
    if (str == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    /* TODO: look at deallocation */
    ret = PY_IMPORT_STRING((char *) str);
    xmlFree(str);
    return (ret);
}

PyObject *
libxml_xmlCharPtrConstWrap(const xmlChar * str)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlCharPtrWrap: str = %s\n", str);
#endif
    if (str == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    /* TODO: look at deallocation */
    ret = PY_IMPORT_STRING((char *) str);
    return (ret);
}

PyObject *
libxml_constcharPtrWrap(const char *str)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlcharPtrWrap: str = %s\n", str);
#endif
    if (str == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    /* TODO: look at deallocation */
    ret = PY_IMPORT_STRING(str);
    return (ret);
}

PyObject *
libxml_constxmlCharPtrWrap(const xmlChar * str)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlCharPtrWrap: str = %s\n", str);
#endif
    if (str == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    /* TODO: look at deallocation */
    ret = PY_IMPORT_STRING((char *) str);
    return (ret);
}

PyObject *
libxml_xmlDocPtrWrap(xmlDocPtr doc)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlDocPtrWrap: doc = %p\n", doc);
#endif
    if (doc == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    /* TODO: look at deallocation */
    ret = PyCapsule_New((void *) doc, (char *) "xmlDocPtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlNodePtrWrap(xmlNodePtr node)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlNodePtrWrap: node = %p\n", node);
#endif
    if (node == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) node, (char *) "xmlNodePtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlURIPtrWrap(xmlURIPtr uri)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlURIPtrWrap: uri = %p\n", uri);
#endif
    if (uri == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) uri, (char *) "xmlURIPtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlNsPtrWrap(xmlNsPtr ns)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlNsPtrWrap: node = %p\n", ns);
#endif
    if (ns == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) ns, (char *) "xmlNsPtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlAttrPtrWrap(xmlAttrPtr attr)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlAttrNodePtrWrap: attr = %p\n", attr);
#endif
    if (attr == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) attr, (char *) "xmlAttrPtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlAttributePtrWrap(xmlAttributePtr attr)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlAttributePtrWrap: attr = %p\n", attr);
#endif
    if (attr == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) attr, (char *) "xmlAttributePtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlElementPtrWrap(xmlElementPtr elem)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlElementNodePtrWrap: elem = %p\n", elem);
#endif
    if (elem == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) elem, (char *) "xmlElementPtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlXPathContextPtrWrap(xmlXPathContextPtr ctxt)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlXPathContextPtrWrap: ctxt = %p\n", ctxt);
#endif
    if (ctxt == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) ctxt, (char *) "xmlXPathContextPtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlXPathParserContextPtrWrap(xmlXPathParserContextPtr ctxt)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlXPathParserContextPtrWrap: ctxt = %p\n", ctxt);
#endif
    if (ctxt == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) ctxt, (char *) "xmlXPathParserContextPtr",
                        NULL);
    return (ret);
}

PyObject *
libxml_xmlParserCtxtPtrWrap(xmlParserCtxtPtr ctxt)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlParserCtxtPtrWrap: ctxt = %p\n", ctxt);
#endif
    if (ctxt == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) ctxt, (char *) "xmlParserCtxtPtr", NULL);
    return (ret);
}

/**
 * libxml_xmlXPathDestructNsNode:
 * cobj: xmlNsPtr namespace node
 * desc: ignored string
 *
 * This function is called if and when a namespace node returned in
 * an XPath node set is to be destroyed. That's the only kind of
 * object returned in node set not directly linked to the original
 * xmlDoc document, see xmlXPathNodeSetDupNs.
 */
#if PY_VERSION_HEX < 0x02070000
static void
libxml_xmlXPathDestructNsNode(void *cap, void *desc ATTRIBUTE_UNUSED)
#else
static void
libxml_xmlXPathDestructNsNode(PyObject *cap)
#endif
{
#ifdef DEBUG
    fprintf(stderr, "libxml_xmlXPathDestructNsNode called %p\n", cobj);
#endif
#if PY_VERSION_HEX < 0x02070000
    xmlXPathNodeSetFreeNs((xmlNsPtr) cap);
#else
    xmlXPathNodeSetFreeNs((xmlNsPtr) PyCapsule_GetPointer(cap, "xmlNsPtr"));
#endif
}

PyObject *
libxml_xmlXPathObjectPtrWrap(xmlXPathObjectPtr obj)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlXPathObjectPtrWrap: ctxt = %p\n", obj);
#endif
    if (obj == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    switch (obj->type) {
        case XPATH_XSLT_TREE: {
            if ((obj->nodesetval == NULL) ||
		(obj->nodesetval->nodeNr == 0) ||
		(obj->nodesetval->nodeTab == NULL)) {
                ret = PyList_New(0);
	    } else {
		int i, len = 0;
		xmlNodePtr node;

		node = obj->nodesetval->nodeTab[0]->children;
		while (node != NULL) {
		    len++;
		    node = node->next;
		}
		ret = PyList_New(len);
		node = obj->nodesetval->nodeTab[0]->children;
		for (i = 0;i < len;i++) {
                    PyList_SetItem(ret, i, libxml_xmlNodePtrWrap(node));
		    node = node->next;
		}
	    }
	    /*
	     * Return now, do not free the object passed down
	     */
	    return (ret);
	}
        case XPATH_NODESET:
            if ((obj->nodesetval == NULL)
                || (obj->nodesetval->nodeNr == 0)) {
                ret = PyList_New(0);
	    } else {
                int i;
                xmlNodePtr node;

                ret = PyList_New(obj->nodesetval->nodeNr);
                for (i = 0; i < obj->nodesetval->nodeNr; i++) {
                    node = obj->nodesetval->nodeTab[i];
                    if (node->type == XML_NAMESPACE_DECL) {
		        PyObject *ns =
			    PyCapsule_New((void *) node,
                                     (char *) "xmlNsPtr",
				     libxml_xmlXPathDestructNsNode);
			PyList_SetItem(ret, i, ns);
			/* make sure the xmlNsPtr is not destroyed now */
			obj->nodesetval->nodeTab[i] = NULL;
		    } else {
			PyList_SetItem(ret, i, libxml_xmlNodePtrWrap(node));
		    }
                }
            }
            break;
        case XPATH_BOOLEAN:
            ret = PY_IMPORT_INT((long) obj->boolval);
            break;
        case XPATH_NUMBER:
            ret = PyFloat_FromDouble(obj->floatval);
            break;
        case XPATH_STRING:
            ret = PY_IMPORT_STRING((char *) obj->stringval);
            break;
        default:
#ifdef DEBUG
            printf("Unable to convert XPath object type %d\n", obj->type);
#endif
            Py_INCREF(Py_None);
            ret = Py_None;
    }
    xmlXPathFreeObject(obj);
    return (ret);
}

xmlXPathObjectPtr
libxml_xmlXPathObjectPtrConvert(PyObject *obj)
{
    xmlXPathObjectPtr ret = NULL;

#ifdef DEBUG
    printf("libxml_xmlXPathObjectPtrConvert: obj = %p\n", obj);
#endif
    if (obj == NULL) {
        return (NULL);
    }
    if (PyFloat_Check (obj)) {
        ret = xmlXPathNewFloat((double) PyFloat_AS_DOUBLE(obj));
    } else if (PyLong_Check(obj)) {
#ifdef PyLong_AS_LONG
        ret = xmlXPathNewFloat((double) PyLong_AS_LONG(obj));
#else
        ret = xmlXPathNewFloat((double) PyInt_AS_LONG(obj));
#endif
#ifdef PyBool_Check
    } else if (PyBool_Check (obj)) {

        if (obj == Py_True) {
          ret = xmlXPathNewBoolean(1);
        }
        else {
          ret = xmlXPathNewBoolean(0);
        }
#endif
    } else if (PyBytes_Check (obj)) {
        xmlChar *str;

        str = xmlStrndup((const xmlChar *) PyBytes_AS_STRING(obj),
                         PyBytes_GET_SIZE(obj));
        ret = xmlXPathWrapString(str);
#ifdef PyUnicode_Check
    } else if (PyUnicode_Check (obj)) {
#if PY_VERSION_HEX >= 0x03030000
        xmlChar *str;
	const char *tmp;
	Py_ssize_t size;

	/* tmp doesn't need to be deallocated */
        tmp = PyUnicode_AsUTF8AndSize(obj, &size);
        str = xmlStrndup((const xmlChar *) tmp, (int) size);
        ret = xmlXPathWrapString(str);
#else
        xmlChar *str = NULL;
        PyObject *b;

	b = PyUnicode_AsUTF8String(obj);
	if (b != NULL) {
	    str = xmlStrndup((const xmlChar *) PyBytes_AS_STRING(b),
			     PyBytes_GET_SIZE(b));
	    Py_DECREF(b);
	}
	ret = xmlXPathWrapString(str);
#endif
#endif
    } else if (PyList_Check (obj)) {
        int i;
        PyObject *node;
        xmlNodePtr cur;
        xmlNodeSetPtr set;

        set = xmlXPathNodeSetCreate(NULL);

        for (i = 0; i < PyList_Size(obj); i++) {
            node = PyList_GetItem(obj, i);
            if ((node == NULL) || (node->ob_type == NULL))
                continue;

            cur = NULL;
            if (PyCapsule_CheckExact(node)) {
#ifdef DEBUG
                printf("Got a Capsule\n");
#endif
                cur = PyxmlNode_Get(node);
            } else if ((PyObject_HasAttrString(node, (char *) "_o")) &&
	               (PyObject_HasAttrString(node, (char *) "get_doc"))) {
		PyObject *wrapper;

		wrapper = PyObject_GetAttrString(node, (char *) "_o");
		if (wrapper != NULL)
		    cur = PyxmlNode_Get(wrapper);
            } else {
#ifdef DEBUG
                printf("Unknown object in Python return list\n");
#endif
            }
            if (cur != NULL) {
                xmlXPathNodeSetAdd(set, cur);
            }
        }
        ret = xmlXPathWrapNodeSet(set);
    } else {
#ifdef DEBUG
        printf("Unable to convert Python Object to XPath");
#endif
    }
    return (ret);
}

PyObject *
libxml_xmlCatalogPtrWrap(xmlCatalogPtr catal)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlNodePtrWrap: catal = %p\n", catal);
#endif
    if (catal == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) catal, (char *) "xmlCatalogPtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlOutputBufferPtrWrap(xmlOutputBufferPtr buffer)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlOutputBufferPtrWrap: buffer = %p\n", buffer);
#endif
    if (buffer == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) buffer, (char *) "xmlOutputBufferPtr", NULL);
    return (ret);
}

PyObject *
libxml_xmlParserInputBufferPtrWrap(xmlParserInputBufferPtr buffer)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlParserInputBufferPtrWrap: buffer = %p\n", buffer);
#endif
    if (buffer == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) buffer, (char *) "xmlParserInputBufferPtr",
                        NULL);
    return (ret);
}

PyObject *
libxml_xmlRegexpPtrWrap(xmlRegexpPtr regexp)
{
    PyObject *ret;

#ifdef DEBUG
    printf("libxml_xmlRegexpPtrWrap: regexp = %p\n", regexp);
#endif
    if (regexp == NULL) {
        Py_INCREF(Py_None);
        return (Py_None);
    }
    ret = PyCapsule_New((void *) regexp, (char *) "xmlRegexpPtr", NULL);
    return (ret);
}
