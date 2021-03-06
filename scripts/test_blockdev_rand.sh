#!/bin/bash
#
# Test block device random IO.

SCRIPT_PATH=$(dirname $0)
EXE_NAME="elbencho"
EXE_PATH="$SCRIPT_PATH/../bin/$EXE_NAME"

unset FILENAMES # filenames generated from $DEVICE
unset RWMIXREAD # rwmixread generated from $READPERCENT

unset DEVICE # blockdev to use (set via cmd line arg)
unset IODEPTH # iodepth for async io (set via cmd line arg)
unset NUMJOBS # number of parallel jobs/processes (set via cmd line arg)
unset READPERCENT # percentage of read accessed (set via cmd line arg
unset BLOCKSIZE # block size for read/write (set via cmd line arg)


# Print usage info and exit
usage()
{
  echo "About:"
  echo "  Test block device direct access performance with random access."
  echo
  echo "Usage:"
  echo "  $0 <DEVICE> <IODEPTH> <NUMJOBS> <READPERCENT> <BLOCKSIZE>"
  echo
  echo "Mandatory Arguments:"
  echo "  DEVICE      Device name in /dev or NVMesh volume name in /dev/nvmesh."
  echo "              (Can be multiple devices space-separated as single arg in quotes.)"
  echo "  IODEPTH     Number of concurrent asynchronous requests per job."
  echo "  NUMJOBS     Number of concurrent threads."
  echo "  READPERCENT Percentage or read access. \"0\" means pure writing and \"100\""
  echo "              means pure reading."
  echo "  BLOCKSIZE   Block size of read/write accesses, e.g. \"4k\" or \"1m\"."
  echo
  echo "Examples:"
  echo "  Check read latency of NVMesh volume /dev/nvmesh/myvol:"
  echo "    $ $0 myvol 1 1 100 4k"
  echo "  Check read IOPS of device /dev/nvme0n1:"
  echo "    $ $0 nvme0n1 16 16 100 4k"
  echo "  Check write throughput of volumes /dev/nvmesh/myvol1 and /dev/nvmesh/myvol2:"
  echo "    $ $0 \"myvol1 myvol2\" 16 16 0 128k"

  exit 1
}

# Parse command line arguments
parse_args()
{
  local OPTIND # local to prevent effects from other subscripts

  while getopts ":h" opt; do
    case "${opt}" in
      h)
        # help
        usage
        ;;
      *)
        # Other option arguments are invalid
        usage
        ;;
    esac
  done

  shift $((OPTIND-1))

  # 5 here for the 5 mandatory args: DEVICE, IODEPTH etc
  if [ $# -ne 5 ]; then
    echo "ERROR: Invalid number of arguments."
    usage
  fi

  # Non-option arguments are assumed to be the mandatory command line args
  DEVICE=$1 # blockdev to use
  IODEPTH=$2 # iodepth for async io
  NUMJOBS=$3 # number of parallel jobs/processes
  READPERCENT=$4 # percentage of read accessed
  BLOCKSIZE=$5 # block size for read/write
  
  if [ "$READPERCENT" -ne 0 ] && [ "$READPERCENT" -ne 100 ]; then
    echo "ERROR: READPERCENT must be either 0 or 100."
    usage
  fi
}


# Check if executable is available and exit if not.
find_executable_or_exit()
{
  if [ ! -f "$EXE_PATH" ]; then
    echo "ERROR: Executable not found. Run \"make\" to create it: $EXE_PATH"
    exit 1
  fi
}

# Prepare filename args for the user-given device names.
# Sets $FILENAMES array.
prepare_arg_filenames()
{
  FILENAMES=()

  for dev in $DEVICE; do
    if [ -e /dev/nvmesh/$dev ]; then
      FILENAMES+=("/dev/nvmesh/$dev")
    elif [ -e /dev/$dev ]; then
      FILENAMES+=("/dev/$dev")
    else
      echo "ERROR: Given device not found: $dev"
      exit 1
    fi
  done
}

# Prepare read/write args based on READPERCENT.
# Sets $RWMIXREAD.
prepare_arg_rwmix()
{
  if [ $READPERCENT -eq 0 ]; then
    RWMIXREAD="-w"
  else
    RWMIXREAD="-r"  
  fi
}

# Check if one of the given devices appears to be mounted and refuse writes if mounted.
# This check does only cover the simple case of the device appearing in /proc/mounts and not cases
# like the device being part of a software RAID, but it's better than nothing to prevent human
# errors.
check_mounted()
{
  if [ "$READPERCENT" -eq 100 ]; then
    return
  fi
  
  for (( i=0; i < ${#FILENAMES[@]}; i++ )); do
    grep "^${FILENAMES[$i]} " /proc/mounts
    if [ $? -eq 0 ]; then
      echo "ERROR: Refusing write test, because block device appears to contain a mounted file" \
        "system: ${FILENAMES[$i]}"
      exit 1
    fi
  done
}

parse_args "$@"
find_executable_or_exit
prepare_arg_filenames
prepare_arg_rwmix
check_mounted

cmd="${EXE_PATH} ${FILENAMES[@]} --iodepth $IODEPTH -t $NUMJOBS $RWMIXREAD -b $BLOCKSIZE "
cmd+=" --direct --rand --randalign --lat --latpercent --randamount 1T --timelimit 600"

echo "COMMAND: ${cmd/"$EXE_PATH"/"$EXE_NAME"}"
echo

$cmd