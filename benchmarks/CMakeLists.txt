add_executable(starter_particles_bench)

target_sources(starter_particles_bench
	PRIVATE
		particles.bench.cc
)

target_link_libraries(starter_particles_bench
	PRIVATE
		starter_module
		starter_language_profile
)
