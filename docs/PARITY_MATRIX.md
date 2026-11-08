# PARITY_MATRIX.md — S642 GENERIC CAPABILITY PARITY (generated)

> **Generated** by `tools/parity_matrix_gen.py` from the SINGLE-SOURCE-OF-TRUTH
> `tools/capability_catalogue.json`. **Do NOT hand-edit this file** — re-run the
> generator when the catalogue advances (the same discipline as `gap_dp_daggen.py`
> → `docs/DAG_controls.md`).
>
> **PARITY — never silent.** Every capability row states our-side status, the
> WPF-emitter-side status, and parity yes/no. A `no` parity MUST carry a named gap.
> The SYNC TOKEN mechanism (S642): a capability lands on OUR side AND the
> WPF-emitter side IN THE SAME STEP, then the matrix is regenerated — no drift.
>
> **COUNT SCOPE — SOFT START:** the catalogue families (10 int / 10 bool / 10 string
> / 10 double / 10 icon / 10 text) are an **initial exemplar, NOT a hard limit**.
> When testing permutations demand more of a family, the catalogue + this matrix +
> the sync check GROW IN PLACE (no new step, no backfill) — recorded as the S642
> count-scope token.

version: 1
counts_are_soft_start: true

| capability | type | kind | tolerance | our side | WPF side | parity | named gap |
|-----------|------|------|-----------|----------|----------|--------|-----------|
| `slot_int_01` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_02` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_03` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_04` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_05` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_06` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_07` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_08` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_09` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_int_10` | int | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_01` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_02` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_03` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_04` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_05` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_06` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_07` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_08` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_09` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_bool_10` | bool | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_string_01` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_02` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_03` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_04` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_05` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_06` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_07` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_08` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_09` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_string_10` | string | Text | 0.08 | introduced | introduced | yes | — |
| `slot_double_01` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_02` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_03` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_04` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_05` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_06` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_07` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_08` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_09` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `slot_double_10` | double | Solid | 0.02 | introduced | introduced | yes | — |
| `icon_01` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_02` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_03` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_04` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_05` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_06` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_07` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_08` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_09` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `icon_10` | icon | Icon | 0.25 | introduced | introduced | yes | — |
| `text_01` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_02` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_03` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_04` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_05` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_06` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_07` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_08` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_09` | text | Text | 0.08 | introduced | introduced | yes | — |
| `text_10` | text | Text | 0.08 | introduced | introduced | yes | — |

## Family summary (soft-start counts)

| family | type | count | kind | default tolerance |
|--------|------|-------|------|-------------------|
| `slot_int` | `int` | 10 | `Solid` | 0.02 |
| `slot_bool` | `bool` | 10 | `Solid` | 0.02 |
| `slot_string` | `string` | 10 | `Text` | 0.08 |
| `slot_double` | `double` | 10 | `Solid` | 0.02 |
| `icon` | `icon` | 10 | `Icon` | 0.25 |
| `text` | `text` | 10 | `Text` | 0.08 |
