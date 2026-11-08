#!/bin/sh

# Script para compilar el demonio de telemetría
# Apunta directamente al Staging Dir dinámico del árbol de OpenWrt 24.10

if [ "$1" = "x86" ]
then
    echo "Compilando para arquitectura x86_64 usando el Staging Dir del árbol principal..."
    
    # 1. Definimos las rutas reales dentro de tu carpeta de trabajo
    export STAGING_DIR=/home/carlos/openwrt/staging_dir
    TOOLCHAIN_BIN=$STAGING_DIR/toolchain-x86_64_gcc-13.3.0_musl/bin
    TARGET_USR=$STAGING_DIR/target-x86_64_musl/usr
    
    rm -f awsTest
    
    # 2. Comando de compilación usando los headers reales que ya generó tu build
    $TOOLCHAIN_BIN/x86_64-openwrt-linux-musl-g++ -O3 -I$TARGET_USR/include -I./includes/ unittest.cpp configmanager.cpp -o unittest -L$TARGET_USR/lib -luci -lubox -pthread
    
elif [ "$1" = "mips" ]
then
    echo "Compilando para arquitectura MIPS usando el Staging Dir del árbol principal..."
    echo "Falta ajustar los nombres de carpeta exactos para MIPS cuando compile esa arquitectura."
fi

echo "Proceso terminado.";
