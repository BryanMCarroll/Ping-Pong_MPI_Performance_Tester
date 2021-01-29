mpi=/home/Bryan/shared/mpich-3.4/
name=perftest

${mpi}bin/mpicc -O3 -std=c11 -o ${name} ${name}.c

if test $? -eq 0; then
    ${mpi}bin/mpiexec --hosts=node1,node2 -np 8 ./${name}
fi
