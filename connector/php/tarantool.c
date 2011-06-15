/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2008 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Alexandre Kalendarev akalend@mail.ru                         |
  | Copyright (c) 2011                                                   |
  +----------------------------------------------------------------------+
*/

/* $Id: header 252479 2008-02-07 19:39:50Z iliaa $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <netinet/in.h>
#include <netdb.h>

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"

#include "tarantool.h"

/* If you declare any globals in php_tarantool.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(tarantool)
*/

/* True global resources - no need for thread safety here */
static int le_tarantool;

zend_class_entry *tarantool_class_entry;

typedef  struct {
		uint32_t type;
		uint32_t len;
		uint32_t request_id;
} Header;

typedef struct _tarantool_object {
	zend_object  zo;
	char * host;		//	tarantool host
	int port;			//  tarantool port
	int bodyLen;		// body len from tuples header	
	int countTuples;	// count tuples
	int readedTuples;	//
	int readed;			// readed byte
	php_stream * stream; 
} tarantool_object;

#define HEADER_SIZE sizeof(Header)

typedef union {
	u_char b;
	uint32_t  i;
} b2i;

typedef	struct {
	uint32_t count;
	u_char data[];
} Tuple;

typedef	struct {
	uint32_t fieldNo;
	int8_t code;
	u_char arg[];
} Operation;


// sizeof(namespaceNo) + sizeof(flag) + sizeof(tuple.count)
#define INSERT_REQUEST_SIZE 12
typedef	struct {
	uint32_t namespaceNo;
	uint32_t flag;
	Tuple tuple;
} InsertRequest;

typedef	struct {
	uint32_t namespaceNo;
	Tuple tuple;
} DeleteRequest;

// sizeof(count) + sizeof(operation.fieldNo) + sizeof(operation.code) +1 // 10
#define UPDATE_REQUEST_SIZE 10 
typedef	struct {
	uint32_t count;
	Operation operation;
} UpdateRequest;



#define SELECT_REQUEST_SIZE 20
typedef	struct {
	uint32_t namespaceNo;
	uint32_t indexNo;
	uint32_t offset;
	uint32_t limit;
	uint32_t count;
	char	 tuples[];
} SelectRequest;

typedef	struct {
	uint32_t len;
	Tuple tuple;
} FTuple;


typedef	struct {
	uint32_t code;			// request error code
	uint32_t tuples_count;	// count of tuples
} SelectResponseBody;


typedef	struct {
	uint32_t size;			// tuple size in bytes
	uint32_t count;			// count elements in tuple	
} SelectResponseTuple;


static void printLine( u_char *p );

static void
leb128_write(char * buf, unsigned long value);

static int
leb128_size(unsigned long value);

static int
leb128_read(char * buf, int size, unsigned long * value);

/* {{{ tarantool_functions[]
 *
 * Every user visible function must have an entry in tarantool_functions[].
 */
const zend_function_entry tarantool_functions[] = {
	 PHP_ME(tarantool_class, __construct, NULL, ZEND_ACC_PUBLIC)	 
	{NULL, NULL, NULL}	
};

zend_function_entry tarantool_class_functions[] = {
	 PHP_ME(tarantool_class, __construct,	NULL, ZEND_ACC_PUBLIC)

	 PHP_ME(tarantool_class, insert,		NULL, ZEND_ACC_PUBLIC)
	 PHP_ME(tarantool_class, select,		NULL, ZEND_ACC_PUBLIC)
	 PHP_ME(tarantool_class, getTuple,		NULL, ZEND_ACC_PUBLIC)	 	 	 
	 PHP_ME(tarantool_class, delete,		NULL, ZEND_ACC_PUBLIC)
	 PHP_ME(tarantool_class, update,		NULL, ZEND_ACC_PUBLIC)
	 PHP_ME(tarantool_class, inc,		NULL, ZEND_ACC_PUBLIC)
	 
	{NULL, NULL, NULL}	
};


/* }}} */

/* {{{ tarantool_module_entry
 */
zend_module_entry tarantool_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"tarantool",
	tarantool_functions,
	PHP_MINIT(tarantool),
	PHP_MSHUTDOWN(tarantool),
	NULL,
	NULL,
	PHP_MINFO(tarantool),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */




#ifdef COMPILE_DL_TARANTOOL
ZEND_GET_MODULE(tarantool)
#endif


static int php_tnt_connect( tarantool_object *ctx TSRMLS_DC) {
			// open stream
		struct timeval tv;
		tv.tv_sec = TARANTOOL_TIMEOUT;
		tv.tv_usec = 0;

		char * errstr = NULL, *hostname = NULL; 
		int   err = 0, hostname_len;

		if (ctx->port) {
			hostname_len = spprintf(&hostname, 0, "tcp://%s:%d", ctx->host, ctx->port);
		} else { 
			hostname_len = spprintf(&hostname, 0, "tcp://%s", ctx->host);
		}

		
		ctx->stream = php_stream_xport_create( hostname, hostname_len,
											   ENFORCE_SAFE_MODE | REPORT_ERRORS,
											   STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
											   NULL, &tv, NULL, &errstr, &err);
	
//		php_printf("stream open code=%d\n", err);
		if (err && errstr) {
			php_printf("stream error: %s\n", errstr);	
			ctx->stream = NULL;
		}		
		
		if (!ctx->stream) {
			return 1;				
		}
		
		return 0;
		
}

static void php_tnt_tuple2data(HashTable *pht, u_char ** p TSRMLS_DC) {
	HashPosition pos; 
	zval **curr;
	
	for(zend_hash_internal_pointer_reset_ex(pht, &pos); 
	  zend_hash_get_current_data_ex(pht, (void **) &curr, &pos) == SUCCESS; 
	  zend_hash_move_forward_ex(pht, &pos)) { 
	
		if (Z_TYPE_PP(curr) == IS_STRING)  {
			char * strval = Z_STRVAL_PP(curr);
			int str_len = Z_STRLEN_PP(curr);
			
			u_char str_shortlen = (u_char)str_len;
			
			**(p++) = str_shortlen;
			memcpy(*p, strval, str_len);
			*p += str_len;
		}
		/*
		if (Z_TYPE_PP(curr) == IS_LONG)  {
		   unsigned long val = Z_LVAL_PP(curr);		
		   
		   u_char leb_size = (u_char)leb128_size( val);	
		   *(p++) = leb_size;
		   leb128_write( (char *)p, val);
		   p += leb_size;	
		} 
		*/
	} 

}

/* {{{ proto tarantool::__construct( string string host=localhost, int port=PORT)
   tarantool constructor */
PHP_METHOD(tarantool_class, __construct)
{
	zval *id;
	tarantool_object *ctx;


	zval** zdata; //??????

	char * host = NULL;
	int host_len = 0;
	long port=0;
			
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O|sl", 
			&id, tarantool_class_entry, &host, &host_len, &port) == FAILURE) {
		return;
	}
		
	ctx = (tarantool_object *)zend_object_store_get_object(id TSRMLS_CC);	
	ctx->host = NULL;
	ctx->port = 0;
	ctx->stream = NULL;
	ctx->bodyLen = 0;
		
	if (host_len > 0) 
		ctx->host = estrdup(host);
	else
		ctx->host = estrdup(TARANTOOL_DEF_HOST);
		
	if (port) 
		ctx->port = port;
	else
		ctx->port = TARANTOOL_DEF_PORT;
}
/* }}} */



/* {{{ proto int tarantool::insert(int namespace, int index, zval tuple);
insert tuple
*/
PHP_METHOD(tarantool_class, insert )
{
	zval *id;
	tarantool_object *ctx;
	long namespace;
	zval * tuple;
	HashTable *pht;
	HashPosition pos; 
	zval **curr;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Ola", &id,
		tarantool_class_entry, &namespace, &tuple) == FAILURE) {
		return;
	}

	ctx = (tarantool_object *)zend_object_store_get_object(id TSRMLS_CC);

	if (!ctx->stream) {
		if (php_tnt_connect(ctx TSRMLS_CC)) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
				"the can't open remote host " ,0 TSRMLS_DC);	
			return;	
		}		
	}

	pht = Z_ARRVAL_P(tuple); 
	int num = zend_hash_num_elements(pht); 
	
	char * out_buf = emalloc(TARANTOOL_BUFSIZE);
	bzero(out_buf, TARANTOOL_BUFSIZE);
	
	Header * header = (Header *) out_buf;
	InsertRequest * insert = (InsertRequest *) (out_buf + HEADER_SIZE);
	
	header->type		= TARANTOOL_INSERT;	
	header->request_id	= TARANTOOL_REQUEST_ID;	
	
	insert->namespaceNo = namespace;	
	insert->tuple.count = num; // count Tuples		
		
	u_char * p = (u_char *) insert->tuple.data;
	
    for(zend_hash_internal_pointer_reset_ex(pht, &pos); 
          zend_hash_get_current_data_ex(pht, (void **) &curr, &pos) == SUCCESS; 
          zend_hash_move_forward_ex(pht, &pos)) { 
        
		if (Z_TYPE_PP(curr) == IS_STRING)  {
			char * strval = Z_STRVAL_PP(curr);
			int str_len = Z_STRLEN_PP(curr);
			
			u_char str_shortlen = (u_char)str_len;
			
			*(p++) = str_shortlen;
			memcpy(p, strval, str_len);
			p += str_len;
		}
		if (Z_TYPE_PP(curr) == IS_LONG)  {
		   unsigned long val = Z_LVAL_PP(curr);		

//		   u_char leb_size = (u_char)leb128_size( val);	
//		   *(p++) = leb_size;
//		   leb128_write( (char *)p, val);
//		   p += leb_size;	
			u_char leb_size = 4;
			*(p++) = leb_size;	
			b2i * pval = (b2i*) p;	
			pval->i = (int) val;
			p += leb_size;	
		} 
    } 

	header->len = INSERT_REQUEST_SIZE + (p-insert->tuple.data); // 12 = 3 * sizeof(int32) :ns, flag & cardinality
	
	// write header
	int len = php_stream_write(ctx->stream, out_buf , sizeof(Header)); // 12
	
	// write tuple	
	p = (u_char*)insert;
	len = php_stream_write(ctx->stream, (char*)p , header->len ); 

	bzero(out_buf, header->len + INSERT_REQUEST_SIZE);
	
	len = php_stream_read(ctx->stream, out_buf, TARANTOOL_BUFSIZE);
	
	if ( *(out_buf+HEADER_SIZE) == '\0') {
		efree(out_buf);
		RETURN_TRUE;
	}		
	
	efree(out_buf);
		
	RETURN_FALSE;
}
/* }}} */



/* {{{ proto int tarantool::select(namespace, index, tuple [, limit=all, offset=0]);
	select tuple use php_streams
*/
PHP_METHOD(tarantool_class, select )
{
	zval *id, * tuple;
	tarantool_object *ctx;
	long ns		= 0;
	long idx	= 0;
	long limit	= 0xffffffff;
	long offset	= 0;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Ollz|ll", &id,
		tarantool_class_entry, &ns, &idx, &tuple, &limit, &offset) == FAILURE) {
		return;
	}

	ctx = (tarantool_object *)zend_object_store_get_object(id TSRMLS_CC);
	if (!ctx)
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"the context is null" ,0 TSRMLS_CC);

	if (!ctx->stream) {
		if (php_tnt_connect(ctx TSRMLS_CC)) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
				"the can't open remote host " ,0 TSRMLS_DC);
			return;	
		}			

	}

	ctx->bodyLen = 0;		
	ctx->countTuples = 0;	
	ctx->readedTuples = 0;	
	ctx->readed	= 0;		

	char out_buf[TARANTOOL_BUFSIZE];
	bzero(out_buf, TARANTOOL_BUFSIZE);
	
	Header * header = (Header *) out_buf;
	SelectRequest * select = (SelectRequest *) out_buf + HEADER_SIZE;
	
	header->type		= TARANTOOL_SELECT;	
	header->request_id	= TARANTOOL_REQUEST_ID;	
	
	select->namespaceNo = ns;	
	select->indexNo		= idx;	
	select->limit		= limit;	
	select->offset		= offset;	
	select->count		= 1;			// �������� ���������� - ���� ������

	u_char * p = (u_char *)select->tuples ; 

	int * count_elements = (int *)p;
			
	switch (Z_TYPE_P(tuple)) {
		case IS_STRING: {				
			*count_elements  = 1;	//!!!! <------- ���-�� ��������� �  �������
			p += sizeof(uint32_t);

				char * strval = Z_STRVAL_P(tuple);
				int str_len = Z_STRLEN_P(tuple);
				u_char str_shortlen = (u_char)str_len;
				
				*(p++) = str_shortlen;
				memcpy(p, strval, str_len);
				p += str_len;

//				printf("tuple: len=%d [%s]\n", str_len, strval );				
			}
			break;

		case IS_LONG: {
			*count_elements  = 1;	//!!!! <------- ���-�� ��������� �  �������
			p += sizeof(uint32_t);

			unsigned long val = Z_LVAL_P(tuple);
//		    u_char leb_size = (u_char)leb128_size( val);	
//		    *(p++) = leb_size;
//		    leb128_write( (char *)p, val);

			u_char leb_size = 4;
			*(p++) = leb_size;	
			b2i * pval = (b2i*) p;	
			pval->i = (int) val;
			p += leb_size;	

//			printf("tuple: int %d\n", val );		
			}
			break;
			
			
		case IS_ARRAY: {
			HashTable *pht;
			HashPosition pos; 
			zval **curr;

			*count_elements  = zend_hash_num_elements(Z_ARRVAL_P(tuple));
			p += sizeof(uint32_t);

			pht = Z_ARRVAL_P(tuple);
			for(zend_hash_internal_pointer_reset_ex(pht, &pos); 
				  zend_hash_get_current_data_ex(pht, (void **)&curr, &pos) == SUCCESS; 
				  zend_hash_move_forward_ex(pht, &pos)) { 
				
				if (Z_TYPE_PP(curr) == IS_STRING)  {
					char * strval = Z_STRVAL_PP(curr);
					int str_len = Z_STRLEN_PP(curr);
		//			printf("tuple: len=%d [%s]", str_len, strval );
					u_char str_shortlen = (u_char)str_len;
					
					*(p++) = str_shortlen;
					memcpy(p, strval, str_len);
					p += str_len;
				}
				if (Z_TYPE_PP(curr) == IS_LONG)  {
					unsigned long val = Z_LVAL_PP(curr);		
				   
//					u_char leb_size = (u_char)leb128_size( val);	
//					*(p++) = leb_size;
//					leb128_write( (char *)p, val);
					u_char leb_size = 4;
					*(p++) = leb_size;	
					b2i * pval = (b2i*) p;	
					pval->i = (int) val;

					p += leb_size;	
				} 
			} 

		}
		break;	
		default : 			
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"tuple: unsuport tuple type" ,0 TSRMLS_CC);
			return;
	}		
		
	u_char * p_end = (u_char *)select->tuples ; 
	header->len = (p - p_end)  +  SELECT_REQUEST_SIZE; //+ HEADER_SIZE ; // 12 = 3 * sizeof(int32) :ns, flag & cardinality
	
	// write header
	int len = php_stream_write(ctx->stream, out_buf, HEADER_SIZE); // 12
	if (len != HEADER_SIZE) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"write header error" ,0 TSRMLS_CC);
	}
	
	len = php_stream_write(ctx->stream, (void*)select , header->len ); 	
	if (len != header->len) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"write request error" ,0 TSRMLS_CC);
	}

	Header responseHeader;
	bzero(&responseHeader, HEADER_SIZE);

	len = php_stream_read(ctx->stream, (void *)&responseHeader, HEADER_SIZE);	
	if (len != HEADER_SIZE) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"read header error" ,0 TSRMLS_CC);
	}	

	ctx->bodyLen = responseHeader.len;
	
	SelectResponseBody response;
	bzero(&response, sizeof(SelectResponseBody));
	
	len = php_stream_read(ctx->stream, (void *) &response, sizeof(SelectResponseBody));
	if (len != sizeof(SelectResponseBody)) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"read body error" ,0 TSRMLS_CC);
	}	

	ctx->readed += sizeof(SelectResponseBody);

	if (response.code) {
			zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C),
			0 TSRMLS_CC,"response code %d", response.code );	
	}
	
	ctx->countTuples = response.tuples_count;
	
	RETURN_LONG(ctx->countTuples);

}
/* }}} */


/* {{{ proto array tarantool::getTuple()
   return one tuple */
PHP_METHOD(tarantool_class, getTuple )
{
	zval *id;
	tarantool_object *ctx;

	int len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id,
		tarantool_class_entry) == FAILURE) {
		return;
	}

	ctx = (tarantool_object *)zend_object_store_get_object(id TSRMLS_CC);
	if (!ctx) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"the context is null" ,0 TSRMLS_CC);
		return;	
	}		
		
	if (!ctx->bodyLen) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"the response body is null" ,0 TSRMLS_CC);
		return;			
	}
	
	if (++ctx->readedTuples > ctx->countTuples) {	
		RETURN_FALSE;
	}
	
	if (ctx->readed >= ctx->bodyLen) {	
		RETURN_FALSE;
	}
	
	
	SelectResponseTuple responseBody;

	if (php_stream_read(ctx->stream, (void*)&responseBody, sizeof(SelectResponseTuple)) !=sizeof(SelectResponseTuple)) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"the read body error" ,0 TSRMLS_CC);
		return;				
	}
	
	ctx->readed += sizeof(SelectResponseTuple);

	u_char * buf = emalloc(responseBody.size);

	if (php_stream_read(ctx->stream, (char*) buf, responseBody.size) != responseBody.size) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"the read tuple error" ,0 TSRMLS_CC);
		return;				
	}
	
	ctx->readed += responseBody.size;
	
	array_init(return_value);	

	char bb2[16];
	int i;
	unsigned long value;
	b2i b;
	b2i * pb;
	u_char* p = buf;
	bool is_string = false;
	
	for(i=0; i < responseBody.count; i++) { // 
		b.i=0;
		b.b = *p;			
		bzero(bb2,16);
		len = b.i;
		memcpy(bb2, p+1, len);
				
		switch (len) {
			case 1 : {
				if ( isprint(*bb2) ) {
//					printf("tuple element '%s' len=%d\n",bb2, len);	
					is_string = true;
				} else {
					is_string = false;
					leb128_read(bb2, len, &value);
//					printf("tuple element(int) %d len=%d\n",value, len);					
				}	
				break;
			}
			case 2 : {
				if ( isprint(*bb2) && isprint(*(bb2+1))) {
//					printf("tuple element '%s' len=%d\n",bb2, len);	
					is_string = true;
				} else {
					is_string = false;				
					leb128_read(bb2, len, &value);
//					printf("tuple element(int) %d len=%d\n",value, len);
				}						
				break;
			}

			case 3 : {
				if ( isprint(*bb2) && isprint(*(bb2+1)) && isprint(*(bb2+2)) ) {
//					printf("tuple element '%s' len=%d\n",bb2, len);	
					is_string = true;
				} else {
					is_string = false;				
					leb128_read(bb2, len, &value);
//					printf("tuple element(int) %d len=%d\n",value, len);					
				}	
				break;
			}

			case 4 : {
				if ( isprint(*bb2) && isprint(*(bb2+1)) && isprint(*(bb2+2)) && isprint(*(bb2+3)) ) {
//					printf("tuple element '%c %c %c'%c len=%d\n",*bb2,*(bb2+1),*(bb2+2),*(bb2+3), len);	
					is_string = true;
				} else {
//					pb = (b2i*) p+2;
//					value = (unsigned long) pb->i;
					leb128_read(bb2, len, &value);
//					printf("tuple element(int) %d len=%d\n",value, len);					
					is_string = false;
				}	
				break;
			}
			default :  {
				is_string = true;
//				printf("tuple element %s len=%d\n",bb2, len);	
			}
		} // end switch


		if (is_string) {
//			php_printf("tuple element '%s' len=%d\n",bb2, len);	
			add_next_index_stringl( return_value, bb2 , len , 1);
		} else	
//			php_printf("tuple element %d\n",bb2, value);	
			add_next_index_long( return_value, value);
						
		p += len+1;		
	}

	efree(buf);
}
/* }}} */

/* {{{ proto int tarantool::delete(key)
   tarantool delete tuple */
PHP_METHOD(tarantool_class, delete)
{
	zval *id;
	tarantool_object *ctx;
	long namespace;

	zval* data; 

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Olz", 
			&id, tarantool_class_entry, &namespace, &data) == FAILURE) {
		return;
	}
		
	ctx = (tarantool_object *)zend_object_store_get_object(id TSRMLS_CC);	
	if (!ctx) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"the context is null" ,0 TSRMLS_CC);
		return;	
	}		
	
	ctx->bodyLen = 0;		
	ctx->countTuples = 0;	
	ctx->readedTuples = 0;	
	ctx->readed	= 0;		

	if (!ctx->stream) {
		if (php_tnt_connect(ctx TSRMLS_CC)) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
				"the can't open remote host " ,0 TSRMLS_DC);	
			return;	
		}		
	}

	char * buf = emalloc(TARANTOOL_BUFSIZE);
	bzero(buf, TARANTOOL_BUFSIZE);
	
	Header * header = (Header *) buf;
	DeleteRequest * delRequest = (DeleteRequest *) buf + HEADER_SIZE;
	
	header->type		= TARANTOOL_DELETE;	
	header->request_id	= TARANTOOL_REQUEST_ID;	

	delRequest->namespaceNo = namespace;	
	delRequest->tuple.count = 1; // count in the Tuples		
		
	u_char * p = (u_char *) delRequest->tuple.data;

	int * count_elements = (int *)p;

	switch (Z_TYPE_P(data)) {
		case IS_STRING: {				

			char * strval = Z_STRVAL_P(data);
			int str_len = Z_STRLEN_P(data);
			u_char str_shortlen = (u_char)str_len;
			
			*(p++) = str_shortlen;
			memcpy(p, strval, str_len);
			p += str_len;
		}
			break;

		case IS_LONG: {

			unsigned long val = Z_LVAL_P(data);
		    u_char leb_size = (u_char)leb128_size( val);	
		    *(p++) = leb_size;
		    leb128_write( (char *)p, val);
		    p += leb_size;	

//			printf("tuple: int %d\n", val );		
			}
			break;

		case IS_ARRAY: {
			HashTable *pht;
			HashPosition pos; 
			zval **curr;

			delRequest->tuple.count = zend_hash_num_elements(Z_ARRVAL_P(data));
			pht = Z_ARRVAL_P(data);
			for(zend_hash_internal_pointer_reset_ex(pht, &pos); 
				  zend_hash_get_current_data_ex(pht, (void **)&curr, &pos) == SUCCESS; 
				  zend_hash_move_forward_ex(pht, &pos)) { 
				
				if (Z_TYPE_PP(curr) == IS_STRING)  {
					char * strval = Z_STRVAL_PP(curr);
					int str_len = Z_STRLEN_PP(curr);
					u_char str_shortlen = (u_char)str_len;
					
					*(p++) = str_shortlen;
					memcpy(p, strval, str_len);
					p += str_len;
				}
				if (Z_TYPE_PP(curr) == IS_LONG)  {
				   unsigned long val = Z_LVAL_PP(curr);		
				   
				   u_char leb_size = (u_char)leb128_size( val);	
				   *(p++) = leb_size;
				   leb128_write( (char *)p, val);
				   p += leb_size;	
				} 
			} 

		}
		break;	
		default : 			
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"tuple: unsuport tuple type" ,0 TSRMLS_CC);
			return;
	}		

	header->len = (p-delRequest->tuple.data) + sizeof(uint32_t) *2 ; // sizeof(int) + sizeof(int)
	
	// write header
	int len = php_stream_write(ctx->stream, buf , HEADER_SIZE); // 12

	// write tuple	
	p = (u_char*)delRequest;
	len = php_stream_write(ctx->stream, (char*)p , header->len ); 

	bzero(buf, header->len + HEADER_SIZE);
	
	len = php_stream_read(ctx->stream, buf, TARANTOOL_BUFSIZE);

	if ( *(buf+HEADER_SIZE) == '\0') {
		int deleted = *(buf+HEADER_SIZE + sizeof(uint32_t));
		efree(buf);
		RETURN_LONG(deleted);
	}		
	
	// TODO ��������� ��� ������ � ������� ������ getLastError()
	efree(buf);		
	RETURN_FALSE;

}
/* }}} */

/* {{{ proto int tarantool::update(int namespace, mixed key, array data);
   tarantool update tuple */
PHP_METHOD(tarantool_class, update)
{
	zval *id;
	tarantool_object *ctx;
	long namespace;

	zval* data;
	 

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Ola", 
			&id, tarantool_class_entry, &namespace, &data) == FAILURE) {
		return;
	}
		
	ctx = (tarantool_object *)zend_object_store_get_object(id TSRMLS_CC);	
	if (!ctx) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"the context is null" ,0 TSRMLS_CC);
		return;	
	}		
	
	ctx->bodyLen = 0;		
	ctx->countTuples = 0;	
	ctx->readedTuples = 0;	
	ctx->readed	= 0;		

	if (!ctx->stream) {
		if (php_tnt_connect(ctx TSRMLS_CC)) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
				"the can't open remote host " ,0 TSRMLS_DC);	
			return;	
		}		
	}

// <insert_request_body> ::= <namespace_no><flags><tuple>
// <update_request_body> ::= <namespace_no><flags><tuple><count><operation>+

// <operation> ::= <field_no><op_code><op_arg>


	HashTable *pht = Z_ARRVAL_P(data); 
	int num = zend_hash_num_elements(pht); 
	
	char * out_buf = emalloc(TARANTOOL_BUFSIZE);
	bzero(out_buf, TARANTOOL_BUFSIZE);
	
	Header * header = (Header *) out_buf;
	InsertRequest * insert = (InsertRequest *) (out_buf + HEADER_SIZE);

	
	header->type		= TARANTOOL_INSERT;	
	header->request_id	= TARANTOOL_REQUEST_ID;	
	
	insert->namespaceNo = namespace;	
	insert->tuple.count = num; // count Tuples		
		
	u_char * p = (u_char *) insert->tuple.data;
	
	zval** curr;
    for(zend_hash_internal_pointer_reset(pht); 
          zend_hash_get_current_data(pht, (void **) &curr) == SUCCESS; 
          zend_hash_move_forward(pht)) { 
        
		if (Z_TYPE_PP(curr) == IS_STRING)  {
			char * strval = Z_STRVAL_PP(curr);
			int str_len = Z_STRLEN_PP(curr);
			
			u_char str_shortlen = (u_char)str_len;
			
			*(p++) = str_shortlen;
			memcpy(p, strval, str_len);
			p += str_len;
		}
		if (Z_TYPE_PP(curr) == IS_LONG)  {
		   unsigned long val = Z_LVAL_PP(curr);		
		   
		   u_char leb_size = (u_char)leb128_size( val);	
		   *(p++) = leb_size;
		   leb128_write( (char *)p, val);
		   p += leb_size;	
		} 
    } 

	
	
	
	header->len = (p-insert->tuple.data) + INSERT_REQUEST_SIZE ; // 12 = 3 * sizeof(int32) :ns, flag & cardinality

	

	// write header
	int len = php_stream_write(ctx->stream, out_buf , HEADER_SIZE); // 12

	// write tuple	
//	p = (u_char*) delRequest;
//	len = php_stream_write(ctx->stream, (char*)p , header->len ); 
//
//	bzero(buf, header->len + HEADER_SIZE);
//	
//	len = php_stream_read(ctx->stream, out_buf, TARANTOOL_BUFSIZE);
//
//	if ( *(buf+HEADER_SIZE) == '\0') {
//		int deleted = *(buf+HEADER_SIZE + sizeof(uint32_t));
//		efree(out_buf);
//		RETURN_LONG(deleted);
//	}		
	
	// TODO ��������� ��� ������ � ������� ������ getLastError()
	efree(out_buf);		
	RETURN_FALSE;

}
/* }}} */


/* {{{ proto int tarantool::inc(int namespace, mixed key, int fieldNo, [data = 1]);
		$tnt->inc(0,'z', array(2 =>5)  );
   tarantool incremental tuple */
PHP_METHOD(tarantool_class, inc)
{
	zval *id;
	tarantool_object *ctx;
	long namespace, fieldNo ;
	zval* key;
	long data = 1;
	 

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Olzl|l", 
			&id, tarantool_class_entry, &namespace, &key, &fieldNo, &data) == FAILURE) {
		return;
	}
		
	ctx = (tarantool_object *)zend_object_store_get_object(id TSRMLS_CC);	
	if (!ctx) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"the context is null" ,0 TSRMLS_CC);
		return;	
	}		
	
	ctx->bodyLen = 0;		
	ctx->countTuples = 0;	
	ctx->readedTuples = 0;	
	ctx->readed	= 0;		

	if (!ctx->stream) {
		if (php_tnt_connect(ctx TSRMLS_CC)) {
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
				"the can't open remote host " ,0 TSRMLS_DC);	
			return;	
		}		
	}

// <insert_request_body> ::= <namespace_no><flags><tuple>
// <update_request_body> ::= <namespace_no><flags><tuple><count><operation>+

// <operation> ::= <field_no><op_code><op_arg>


	char * out_buf = emalloc(TARANTOOL_BUFSIZE);
	bzero(out_buf, TARANTOOL_BUFSIZE);

	
	Header * header = (Header *) out_buf;

	
	header->type		= TARANTOOL_UPDATE;	
	header->request_id	= TARANTOOL_REQUEST_ID;	
	
	InsertRequest * insert = (InsertRequest *) (out_buf + HEADER_SIZE);

	insert->namespaceNo = namespace;	
	insert->tuple.count = 1; 
		
	u_char * p = (u_char *) insert->tuple.data;
		
	switch (Z_TYPE_P(key)) {
		case IS_STRING: {				
//			*count_elements  = 1;	//!!!! <------- ���-�� ��������� �  �������
//			p += sizeof(uint32_t);

				char * strval = Z_STRVAL_P(key);
				int str_len = Z_STRLEN_P(key);
				u_char str_shortlen = (u_char)str_len;
				
				*(p++) = str_shortlen;
				memcpy(p, strval, str_len);
				p += str_len;

				printf("tuple: len=%d [%s]\n", str_len, strval );				
			}
			break;

		case IS_LONG: {
//			*count_elements  = 1;	//!!!! <------- ���-�� ��������� �  �������
//			p += sizeof(uint32_t);

			unsigned long val = Z_LVAL_P(key);

		    u_char leb_size = 4; //(u_char)leb128_size( val);	
		    *(p++) = leb_size;
			
			b2i * pb = p;
			pb->i = (uint32_t) val;		
//		    leb128_write( (char *)p, val);
		    p += leb_size;	

//			printf("tuple: int %d\n", val );		
			}
			break;

		default : 			
			zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"unsupport key type" ,0 TSRMLS_CC);
			return;
	}		

	const int insertLen = p-insert->tuple.data;

	UpdateRequest  * incRequest = (UpdateRequest*) p;
	incRequest->count = 1;
	
	incRequest->operation.code = TARANTOOL_OP_ADD;
	
	
	incRequest->operation.fieldNo = fieldNo;

	u_char leb_size = '\4';
	if (data == 1) {
		incRequest->operation.arg[0] = leb_size;
		incRequest->operation.arg[1] = '\1';
		incRequest->operation.arg[2] = '\0';
		incRequest->operation.arg[3] = '\0';
		incRequest->operation.arg[4] = '\0';
		
	} else {
		p = incRequest->operation.arg;
		*(p++) = leb_size;
		b2i * pb = (b2i*) p; 
		pb->i = (int) data;
		p += leb_size;		
	}

	header->len = INSERT_REQUEST_SIZE + insertLen + UPDATE_REQUEST_SIZE + (int)leb_size; 
	
	// write header
	int len = php_stream_write(ctx->stream, out_buf , HEADER_SIZE); // 12
	if (len!=HEADER_SIZE) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"error write header" ,0 TSRMLS_CC);
		return;
	}

//	write tuple	
	p = (u_char*) out_buf+HEADER_SIZE;
	len = php_stream_write(ctx->stream, (char*)p , header->len ); //
	if (len != header->len) {
		zend_throw_exception(zend_exception_get_default(TSRMLS_C),
			"error write body" ,0 TSRMLS_CC);
		return;	
	}	

	bzero(out_buf, header->len + HEADER_SIZE);
//	
	len = php_stream_read(ctx->stream, out_buf, TARANTOOL_BUFSIZE);

	if ( *(out_buf+HEADER_SIZE) == '\0') {
		efree(out_buf);
		RETURN_TRUE;
	}		
	
	// TODO ��������� ��� ������ � ������� ������ getLastError()
	efree(out_buf);		
	RETURN_FALSE;

}
/* }}} */


static void printLine( u_char *p ) {
	u_char b[4];
	memcpy(b, p, 4);
	php_printf("%x %x %x %x\t\t", b[0], b[1], b[2], b[3]);
}

static void
leb128_write(char * buf, unsigned long value)
{
	if (value >= (1 << 7)) {
		if (value >= (1 << 14)) {
			if (value >= (1 << 21)) {
				if (value >= (1 << 28))
					*(buf++) = (value >> 28) | 0x80;
				*(buf++) = (value >> 21) | 0x80;
			}
			*(buf++) = ((value >> 14) | 0x80);
		}
		*(buf++) = ((value >> 7) | 0x80);
	}
	*(buf++) = ((value) & 0x7F);

}

static int
leb128_read(char * buf, int size, unsigned long * value)
{
	*value = 0;

	if (size < 1)
		return -1;

	if (!(buf[0] & 0x80)) {

		*value = buf[0] & 0x7f;
		return 1;
	}

	if (size < 2)
		return -1;

	if (!(buf[1] & 0x80)) {

		*value = (buf[0] & 0x7f) << 7 |
		         (buf[1] & 0x7f);
		return 2;
	}

	if (size < 3)
		return -1;

	if (!(buf[2] & 0x80)) {

		*value = (buf[0] & 0x7f) << 14 |
				 (buf[1] & 0x7f) << 7  |
				 (buf[2] & 0x7f);
		return 3;
	}

	if (size < 4)
		return -1;

	if (!(buf[3] & 0x80)) {

		*value = (buf[0] & 0x7f) << 21 |
				 (buf[1] & 0x7f) << 14 |
				 (buf[2] & 0x7f) << 7  |
				 (buf[3] & 0x7f);
		return 4;
	}

	if (size < 5)
		return -1;

	if (!(buf[4] & 0x80)) {

		*value = (buf[0] & 0x7f) << 28 |
		         (buf[1] & 0x7f) << 21 |
				 (buf[2] & 0x7f) << 14 |
				 (buf[3] & 0x7f) << 7  |
				 (buf[4] & 0x7f);
		return 5;
	}

	return -1;
}


static int
leb128_size(unsigned long value)
{
	if (value < (1 << 7))
		return 1;

	if (value < (1 << 14))
		return 2;

	if (value < (1 << 21))
		return 3;

	if (value < (1 << 28))
		return 4;

	return 5;
}

static void 
tarantool_dtor(void *object TSRMLS_DC)
{			
	tarantool_object *ctx = (tarantool_object*)object;

	if (ctx) {
		if (ctx->stream) {
			php_stream_close(ctx->stream); 
			ctx->stream = NULL;
		}	

		if (ctx->host) {
			efree(ctx->host);		
		}
		
	}

	zend_object_std_dtor(&(ctx->zo) TSRMLS_CC);	 	  	   

	efree(object);

}

static zend_object_value tarantool_ctor(zend_class_entry *ce TSRMLS_DC)
{

	zend_object_value new_value;
	tarantool_object* obj = (tarantool_object*)emalloc(sizeof(tarantool_object));
	zend_object_std_dtor(&(obj->zo) TSRMLS_CC);

	memset(obj, 0, sizeof(tarantool_object));

	zend_object_std_init(&obj->zo, ce TSRMLS_CC);

	new_value.handle = zend_objects_store_put(obj, (zend_objects_store_dtor_t)zend_objects_destroy_object,
		(zend_objects_free_object_storage_t)tarantool_dtor, NULL TSRMLS_CC);
	new_value.handlers = zend_get_std_object_handlers();

	return new_value;
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(tarantool)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Tarantool", tarantool_class_functions);
	ce.create_object = tarantool_ctor;
	tarantool_class_entry = zend_register_internal_class(&ce TSRMLS_CC);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(tarantool)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(tarantool)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "tarantool support", "enabled");
	php_info_print_table_row(2,		"default host", TARANTOOL_DEF_HOST);
	php_info_print_table_row(2,		"default port", TARANTOOL_DEF_PORT);
	php_info_print_table_row(2,		"timeout in sec",TARANTOOL_TIMEOUT);
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */