# Evidence archive

No retired GCC investigation evidence is stored in this repository. Public
Bugzilla and mailing-list records are linked from [`BUGS.md`](BUGS.md).

## Bugzilla access

The Bugzilla HTML UI blocks automated fetches with an Anubis bot-check
interstitial. Plain curl and the REST API work:
`https://gcc.gnu.org/bugzilla/rest/bug/<id>/comment` and
`https://gcc.gnu.org/bugzilla/rest/bug?quicksearch=...`. GCC policy still
requires a duplicate search in a real browser before filing.
