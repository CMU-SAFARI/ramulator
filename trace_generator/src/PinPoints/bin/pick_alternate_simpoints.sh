#!/bin/sh
# usage pick_alternate_simpoints.sh rank# t.simpoints t.labels
# Goal: Create the rank'th best simpoints file (rank 1 the best,
# rank 2 the second best...)
# NOTE: rank==1 should generate the same simppoints file that is passed in.
ERROR()
{
  echo "Usage : pick_alternate_simpoints.sh rank t.simpoints t.labels"
  echo "   Goal: Create the rank'th best simpoints file (rank 1 the best,"
  echo "   rank 2 the second best...)"
  echo "   NOTE: rank==1 should generate the same simppoints file that is passed in."
  exit
}
if  [ $# != 3 ];  then
    echo "Not enough arguments!"
    ERROR
fi

for slice in `cat $2 | awk '{print $1}'`
do
    #echo "slice=" $slice
    slice_plus_1=`echo $slice+1|bc`
    #echo "slice_plus_1=" $slice_plus_1
    # Find the cluster this slice belongs to
    # Arg 3 is the labels file with n'th line standing
    # for the n'th slice showing the cluster and the distance
    # from the cluster centroid for the n'th slice
    cluster=`head -$slice_plus_1 $3 | tail -1 | awk '{print $1}'`
    #echo "cluster=" $cluster
    # First filter the labels for this cluster, then sort the entries (by
    # distance from the centroid) and then pick the distance for the
    # rank'th entry
    dist=`grep "^$cluster " $3 | sort -n | head -$1 | tail -1 | awk '{print $2}'`
    # There could be multiple slices with the same distance; pick the first
    nextpp=`grep -n "^$cluster $dist" $3 | head -1 | sed '/:.*$/s///g'`
    nextpp_slice=`echo $nextpp-1 | bc`
    echo $nextpp_slice $cluster
done
