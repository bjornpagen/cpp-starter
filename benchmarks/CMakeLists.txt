add_executable(starter_particles_bench)

target_sources(starter_particles_bench
	PRIVATE
		starter.particles.bench.cpp
)

target_link_libraries(starter_particles_bench
	PRIVATE
		starter_particles
		starter_integrate
		starter_simd
		starter_language_profile
)
