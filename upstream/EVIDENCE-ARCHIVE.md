# Evidence archive

In-tree technical evidence lives only in
[`gcc-fixincludes-darwin-rsize-t/`](gcc-fixincludes-darwin-rsize-t/). Other
filed defects are Bugzilla pointers in [`BUGS.md`](BUGS.md).

## Bugzilla access

The Bugzilla HTML UI blocks automated fetches with an Anubis bot-check
interstitial. Plain curl and the REST API work:
`https://gcc.gnu.org/bugzilla/rest/bug/<id>/comment` and
`https://gcc.gnu.org/bugzilla/rest/bug?quicksearch=...`. GCC policy still
requires a duplicate search in a real browser before filing.
