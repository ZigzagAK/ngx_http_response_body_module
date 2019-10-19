#!/bin/bash

# Copyright, Aleksey Konovkin (alkon2000@mail.ru)
# BSD license type

if [ "$1" == "" ]; then
  echo "build.sh <clean/clean_all> <download/download_all> <build>"
  exit 0
fi

download=0
download_only=0
download_all=0
build_deps=0
clean_all=0
compile=0
build_only=0
make_clean=0

DIR="$(pwd)"
DIAG_DIR="diag"
VCS_PATH=${DIR%/*/*}

VERSION="1.17.4"
PCRE_VERSION="8.40"
ZLIB_VERSION="1.2.11"

SUFFIX=""

if [ "$BUILD_DIR" == "" ]; then
  BUILD_DIR="$DIR/build"
fi

if [ "$INSTALL_DIR" == "" ]; then
  INSTALL_DIR="$DIR/install"
fi

if [ "$ERR_LOG" == "" ]; then
  ERR_LOG=/dev/stderr
else
  ERR_LOG=$BUILD_DIR/$ERR_LOG
fi

if [ "$BUILD_LOG" == "" ]; then
  BUILD_LOG=/dev/stdout
else
  BUILD_LOG=$BUILD_DIR/$BUILD_LOG
fi

[ -e "$BUILD_DIR" ] || mkdir -p $BUILD_DIR

export ZLIB_PREFIX="$BUILD_DIR/deps/zlib"
export PCRE_PREFIX="$BUILD_DIR/deps/pcre"

export LD_LIBRARY_PATH="-L$PCRE_PREFIX/lib:$ZLIB_PREFIX/lib"

ADDITIONAL_INCLUDES="-I$PCRE_PREFIX/include -I$ZLIB_PREFIX/include"
ADDITIONAL_LIBS="-L$PCRE_PREFIX/lib -L$ZLIB_PREFIX/lib"

function clean() {
  rm -rf install  2>>$ERR_LOG
  if [ $clean_all -eq 1 ]; then
    rm -rf $BUILD_DIR  2>>$ERR_LOG
  else
    rm -rf $(ls -1d $BUILD_DIR/* 2>>$ERR_LOG | grep -v deps)    2>>$ERR_LOG
  fi
  if [ $download_all -eq 1 ]; then
    rm -rf src 2>>$ERR_LOG
  fi
}

doclean=0
dobuild=0

for i in "$@"
do
  if [ "$i" == "download" ]; then
    download=1
  fi

  if [ "$i" == "download_all" ]; then
    download=1
    download_all=1
  fi

  if [ "$i" == "clean_all" ]; then
    clean_all=1
    doclean=1
  fi

  if [ "$i" == "build" ]; then
    dobuild=1
  fi

  if [ "$i" == "build_only" ]; then
    dobuild=1
    build_only=1
  fi

  if [ "$i" == "clean" ]; then
    doclean=1
  fi

  if [ "$i" == "compile" ]; then
    compile=1
  fi
done

if [ $doclean -eq 1 ]; then
  clean
fi

if [ $download -eq 1 ] && [ $dobuild -eq 0 ]; then
  download_only=1
fi

if [ $download -eq 0 ] && [ $dobuild -eq 0 ]; then
    if [ $make_components -eq 0 ]; then 
      exit 0
    fi
fi


current_os=`uname`
if [ "$current_os" = "Linux" ]; then
  platform="linux"
  arch=`uname -p`
  shared="so"
  if [ -e /etc/redhat-release ]; then
    vendor='redhat'
    ver=`cat /etc/redhat-release | sed -e 's#[^0-9]##g' -e 's#7[0-2]#73#'`
    if [ $ver -lt 50 ]; then
      os_release='4.0'
    elif [ $ver -lt 60 ]; then
      os_release='5.0'
    elif [ $ver -lt 70 ]; then
      os_release='6.0'
    else
      os_release='7.0'
    fi
    if [ "$arch" != "x86_64" ]; then
      arch='i686'
    fi
    DISTR_NAME=$vendor-$platform-$os_release-$arch
  else
    vendor=$(uname -r)
    DISTR_NAME=$vendor-$platform-$arch
  fi
fi
if [ "$current_os" = "Darwin" ]; then
  platform="macos"
  arch=`uname -m`
  vendor="apple"
  shared="dylib"
  CFLAGS="-arch x86_64"
  DISTR_NAME=$vendor-$platform-$arch
fi

case $platform in
  linux)
    # platform has been recognized
    ;;
  macos)
    # platform has been recognized
    ;;
  *)
    echo "I do not recognize the platform '$platform'." | tee -a $BUILD_LOG
    exit 1;;
esac

if [ -z "$BUILD_VERSION" ]; then
    BUILD_VERSION="develop"
fi

function build_pcre() {
  echo "Build PCRE" | tee -a $BUILD_LOG
  cd pcre-$PCRE_VERSION
  ./configure --prefix="$PCRE_PREFIX" --libdir="$PCRE_PREFIX/lib" >> $BUILD_LOG 2>>$ERR_LOG
  make -j 8 >> $BUILD_LOG 2>>$ERR_LOG
  r=$?
  if [ $r -ne 0 ]; then
    exit $r
  fi
  make install >> $BUILD_LOG 2>>$ERR_LOG
  cd ..
}

function build_zlib() {
  echo "Build ZLIB" | tee -a $BUILD_LOG
  cd zlib-$ZLIB_VERSION
  ./configure --prefix="$ZLIB_PREFIX" --libdir="$ZLIB_PREFIX/lib" >> $BUILD_LOG 2>>$ERR_LOG
  make -j 8 >> $BUILD_LOG 2>>$ERR_LOG
  r=$?
  if [ $r -ne 0 ]; then
    exit $r
  fi
  make install >> $BUILD_LOG 2>>$ERR_LOG
  cd ..
}

function build_release() {
  cd nginx-$VERSION
  if [ $build_only -eq 0 ]; then
    make clean >> $BUILD_LOG 2>>$ERR_LOG
    echo "Configuring release nginx-$VERSION" | tee -a $BUILD_LOG
    ./configure --prefix="$INSTALL_DIR/nginx-$VERSION" \
                $EMBEDDED_OPTS \
                --with-stream \
                --with-cc-opt="$ADDITIONAL_INCLUDES -O0 -g" \
                --with-ld-opt="$ADDITIONAL_LIBS" \
                --add-module=../echo-nginx-module \
                --add-module=../.. >> $BUILD_LOG 2>>$ERR_LOG

    r=$?
    if [ $r -ne 0 ]; then
      exit $r
    fi
  fi

  echo "Build release nginx-$VERSION" | tee -a $BUILD_LOG
  make -j 8 >> $BUILD_LOG 2>>$ERR_LOG

  r=$?
  if [ $r -ne 0 ]; then
    exit $r
  fi
  make install >> $BUILD_LOG 2>>$ERR_LOG
  cd ..
}

function gitclone() {
  LD_LIBRARY_PATH="" git clone $1 >> $BUILD_LOG 2> /tmp/err
  if [ $? -ne 0 ]; then
    cat /tmp/err
    exit 1
  fi
}

function gitcheckout() {
  git checkout $1 >> $BUILD_LOG 2> /tmp/err
  if [ $? -ne 0 ]; then
    cat /tmp/err
    exit 1
  fi
}

function download_module() {
  if [ -e $DIR/../$2 ]; then
    echo "Get $DIR/../$2"
    dir=$(pwd)
    cd $DIR/..
    tar zcf $dir/$2.tar.gz $(ls -1d $2/* | grep -vE "(install$)|(build$)|(download$)|(.git$)")
    cd $dir
  else
    if [ $download -eq 1 ] || [ ! -e $2.tar.gz ]; then
      echo "Download $2 branch=$3"
      curl -s -L -o $2.zip https://github.com/$1/$2/archive/$3.zip
      unzip -q $2.zip
      mv $2-* $2
      tar zcf $2.tar.gz $2
      rm -rf $2-* $2 $2.zip
    fi
  fi
}

function download_dep() {
  if [ $download -eq 1 ] || [ ! -e $2-$3.tar.gz ]; then
    if [ $download_all -eq 1 ] || [ ! -e $2-$3.tar.gz ]; then
      echo "Download $2-$3.$4" | tee -a $BUILD_LOG
      LD_LIBRARY_PATH="" curl -s -L -o $2-$3.tar.gz $1/$2-$3.$4
      echo "$1/$2-$3.$4" > $2.log
    else
      echo "Get $2-$3.tar.gz" | tee -a $BUILD_LOG
    fi
  else
    echo "Get $2-$3.tar.gz" | tee -a $BUILD_LOG
  fi
}

function extract_downloads() {
  cd download

  for d in $(ls -1 *.tar.gz)
  do
    echo "Extracting $d" | tee -a $BUILD_LOG
    tar zxf $d -C $BUILD_DIR --keep-old-files 2>>$ERR_LOG
  done

  cd ..
}

function download() {
  mkdir -p $BUILD_DIR        2>>$ERR_LOG
  mkdir $BUILD_DIR/deps      2>>$ERR_LOG

  mkdir download             2>>$ERR_LOG

  cd download

  download_dep http://nginx.org/download                                           nginx     $VERSION           tar.gz
  download_dep http://ftp.cs.stanford.edu/pub/exim/pcre                            pcre      $PCRE_VERSION      tar.gz
  download_dep http://zlib.net                                                     zlib      $ZLIB_VERSION      tar.gz

  download_module openresty   echo-nginx-module                master

  cd ..
}

function install_file() {
  echo "Install $1" | tee -a $BUILD_LOG
  if [ ! -e "$INSTALL_DIR/nginx-$VERSION/$2" ]; then
    mkdir -p "$INSTALL_DIR/nginx-$VERSION/$2"
  fi
  if [ "$4" == "" ]; then
    if [ "$3" == "" ]; then
      cp -r $1 "$INSTALL_DIR/nginx-$VERSION/$2/"
    else
      cp -r $1 "$INSTALL_DIR/nginx-$VERSION/$2/$3"
    fi
  else
    echo $4 > "$INSTALL_DIR/nginx-$VERSION/$2/$3"
  fi
}

function install_gzip() {
  echo "Install $1" | tee -a $BUILD_LOG
  if [ ! -e "$INSTALL_DIR/nginx-$VERSION/$2" ]; then
    mkdir -p "$INSTALL_DIR/nginx-$VERSION/$2"
  fi
  if [ "$4" == "" ]; then
    if [ "$3" == "" ]; then
      tar zxf $1 -C "$INSTALL_DIR/nginx-$VERSION/$2/"
    else
      tar zxf $1 -C "$INSTALL_DIR/nginx-$VERSION/$2/$3"
    fi
  else
    echo $4 > "$INSTALL_DIR/nginx-$VERSION/$2/$3"
  fi
}

function install_files() {
  for f in $(ls $1)
  do
    install_file $f $2
  done
}

function build() {
  cd $BUILD_DIR

  if [ $build_deps -eq 1 ] || [ ! -e deps/zlib ]; then
    build_zlib
  fi
  if [ $build_deps -eq 1 ] || [ ! -e deps/pcre ]; then
    build_pcre
  fi

  build_release

  install_files "$ZLIB_PREFIX/lib/libz.$shared*"             lib

  install_files "$PCRE_PREFIX/lib/libpcre.$shared*"          lib
  install_files "$PCRE_PREFIX/lib/libpcreposix.$shared*"     lib

  chmod 755 $INSTALL_DIR/nginx-$VERSION/lib/*.$shared*

  cd $DIR
}

if [ $build_only -eq 0 ]; then
  clean
fi
download
if [ $download_only -eq 0 ]; then
  if [ $build_only -eq 0 ]; then
    extract_downloads
  fi
  build
fi

cd "$DIR"

kernel_name=$(uname -s)
kernel_version=$(uname -r)

cd install

tar zcvf nginx-$VERSION$SUFFIX-$kernel_name-$kernel_version.tar.gz nginx-$VERSION$SUFFIX
rm -rf nginx-$VERSION$SUFFIX

cd ..

exit $r
