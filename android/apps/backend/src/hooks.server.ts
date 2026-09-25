import type { Handle } from '@sveltejs/kit';

const CORS_HEADERS = {
	'Access-Control-Allow-Origin': '*',
	'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
	'Access-Control-Allow-Headers': 'Content-Type, Authorization',
	'Access-Control-Max-Age': '86400'
};

function isApiRoute(pathname: string): boolean {
	return pathname.startsWith('/api/');
}

export const handle: Handle = async ({ event, resolve }) => {
	if (isApiRoute(event.url.pathname) && event.request.method === 'OPTIONS') {
		return new Response(null, {
			status: 204,
			headers: CORS_HEADERS
		});
	}

	const response = await resolve(event);

	if (isApiRoute(event.url.pathname)) {
		for (const [key, value] of Object.entries(CORS_HEADERS)) {
			response.headers.set(key, value);
		}
	}

	return response;
};
