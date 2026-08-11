import m;

int main()
{
	auto s = n::make("x");
	return s ? static_cast<int>(s->describe().size()) : 1;
}
