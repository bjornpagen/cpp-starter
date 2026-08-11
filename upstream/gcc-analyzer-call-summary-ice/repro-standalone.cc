struct payload {
	int x;
	payload(int v) : x(v) {}
	payload(const payload& o) : x(o.x) {}
};

__attribute__((noinline)) static payload take(payload& v) {
	return v;
}

static void run(payload& out, payload& v) {
	out = take(v);
}

void caller1(payload& t, payload& v) {
	run(t, v);
}

void caller2(payload& t, payload& v) {
	run(t, v);
}
