#ifndef TARANTOOL_COMPHINT_H_INCLUDED
#define TARANTOOL_COMPHINT_H_INCLUDED

typedef struct compare_hint
{
	uint32_t fields_hit;
	uint32_t current_field_hit;
} compare_hint;

inline void
min_hint(const compare_hint *h1, const compare_hint *h2, compare_hint *res)
{
	if (h1->fields_hit < h2->fields_hit) {
		*res = *h1;
	} else if (h1->fields_hit > h2->fields_hit) {
		*res = *h2;
	} else {
		res->fields_hit = h1->fields_hit;
		res->current_field_hit =
			h1->current_field_hit < h2->current_field_hit ?
			h1->current_field_hit : h2->current_field_hit;
	}
}


#endif
