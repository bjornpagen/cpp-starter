# Runnable examples over the exported module surface: dialect code, same
# rules as src/ and tests/.
add_executable(starter_httpd)

target_sources(starter_httpd
	PRIVATE
		httpd.cc
)

target_link_libraries(starter_httpd
	PRIVATE
		starter_module
		starter_language_profile
)
