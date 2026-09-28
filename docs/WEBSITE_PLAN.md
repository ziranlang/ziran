# Ziran website plan

## Goal

A visitor should be able to answer four questions in order: what Ziran is,
whether it fits their project, how to run a first program, and where to find
the exact reference for a feature. The site must describe the compiler's
current implementation, not planned capabilities.

## Reference sites

- [Go](https://go.dev/) leads with a concrete purpose, a Get Started action,
  visible download path, runnable example, use cases, and clear routes to
  tutorials and package documentation.
- [Rust](https://rust-lang.org/) separates its value proposition from the
  install and learn routes. Its learning page distinguishes tutorials,
  examples, standard library documentation, and detailed reference material.
- [Zig](https://ziglang.org/) makes version and development status visible,
  then links the getting-started guide, language reference, standard library,
  and platform support.

Ziran is earlier in its lifecycle, so it should adopt the navigation and
learning patterns without claiming a package ecosystem, browser playground,
prebuilt releases, or production adopters it does not have.

## Initial changes

1. **Homepage:** lead with what a developer can do, show a complete code
   sample and result, then explain language features and the supported output
   paths. Keep experimental status visible.
2. **Learning path:** separate the first-program guide from a documentation
   hub, and add complete examples with commands checked against the current
   toolchain.
3. **Reference:** keep syntax, API, and status as distinct destinations. Link
   them from the docs hub and from the relevant homepage sections.
4. **Trust:** correct stale Vec and bundle-version claims; use language that
   distinguishes checked source from backend-specific support.
5. **Navigation and presentation:** use consistent site navigation and footer,
   mobile layouts, code copying, clear focus states, and readable code blocks.
6. **Domain:** make `https://ziran-lang.org/` the canonical address in site
   metadata, sitemap, and repository documentation.

## Next content work

1. Add a tutorial that builds a small useful command-line program once the
   CLI and standard-library surface for printing and arguments is stable.
2. Generate the standard-library reference from source declarations, so API
   pages cannot silently lag changes to `std/`.
3. Publish release-specific downloads and installation instructions when
   reproducible artifacts exist. Until then, the site should say “build from
   source.”
4. Add an in-browser runner only after the portable compiler and runtime can
   run safely in a browser; a static code example is more honest today.
5. Add real project stories when public Ziran projects can be linked and their
   claims verified.

## Cloudflare cutover

The repository's publish directory is `site/`. The existing Cloudflare
Pages project is `ziran`, with `master` as its production branch. It is a
direct-upload project, so Cloudflare does not build from Git: the
`.github/workflows/site.yml` workflow deploys `site/` on every push to
`master` that changes it, using the repository secrets
`CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID`. To publish by hand, run
`npx wrangler pages deploy site --project-name=ziran --branch=master`.
`ziran-lang.org` is attached to the project as an active custom domain,
and its proxied apex CNAME points to `ziran-bwc.pages.dev`.

The supplied Cloudflare token manages DNS and Pages but does not have
permission for zone Redirect Rules. The deployed `site/_worker.js` handles
the migration inside Pages instead: it sends a permanent 301 from
`ziran.kryonlabs.com` to `ziran-lang.org` with the path and query intact,
and serves static assets on the new hostname. It reproduces the security
headers from `site/_headers`, which Pages does not apply to Function
responses.

Verify the homepage, guide, examples, reference pages, sitemap, and
robots.txt at the new hostname. Check that old URLs redirect to their
corresponding new URLs. Submit `https://ziran-lang.org/sitemap.xml`
to the search engines used by the project. Configure `www.ziran-lang.org`
only if a www hostname is wanted.

## Success checks

- A new visitor reaches a running program from the homepage in one click.
- Every code sample on the quick start and examples pages checks and yields
  its stated output.
- Every local navigation link resolves at desktop and mobile sizes.
- The site never presents a planned capability as available.
- The new hostname is the canonical address, with old URLs returning 301.
