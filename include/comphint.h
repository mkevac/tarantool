#ifndef TARANTOOL_COMPHINT_H_INCLUDED
#define TARANTOOL_COMPHINT_H_INCLUDED

typedef struct compare_hint
{
	uint32_t fields_hit;
	uint32_t current_field_hit;
} compare_hint;

void min_hint(const compare_hint *h1, const compare_hint *h2,
	      compare_hint *res);


#endif
