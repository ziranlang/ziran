export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.hostname === 'ziran.kryonlabs.com') {
      url.protocol = 'https:';
      url.hostname = 'ziran-lang.org';
      return Response.redirect(url.toString(), 301);
    }
    // The single standard library page became one page per module.
    if (url.pathname === '/std' || url.pathname === '/std.html') {
      return Response.redirect(`${url.origin}/modules#std`, 301);
    }

    const response = await env.ASSETS.fetch(request);
    const headers = new Headers(response.headers);
    headers.set('X-Content-Type-Options', 'nosniff');
    headers.set('Referrer-Policy', 'strict-origin-when-cross-origin');
    headers.set('Permissions-Policy', 'camera=(), microphone=(), geolocation=()');
    return new Response(response.body, {
      status: response.status,
      statusText: response.statusText,
      headers,
    });
  },
};
