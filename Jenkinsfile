pipeline {
    agent none
    stages {
        stage('Test NUT') {
            parallel {
                stage("BuildAndTest-GCC") {
                    matrix {
                    agent { label "OS=${PLATFORM} && GCCVER=${GCCVER}" }
                        axes {
                            axis {
                                name 'PLATFORM'
                                values 'linux', 'openindiana'
                            }
                            axis {
                                name 'GCCVER'
                                values '4.4.4', '4.8', '4.9', '5', '6', '7', '8', '10'
                            }
                            axis {
                                name 'STDVER'
                                values '99', '11', '17', '2x'
                            }
                            axis {
                            name 'BUILD_WARNOPT'
                                values 'hard'
                            }
                            axis {
                                name 'STD'
                                values 'gnu' //, 'c'
                            }
                        }

                        excludes {
                            exclude {
                                axis {
                                    name 'PLATFORM'
                                    values 'openindiana'
                                }
                                axis {
                                    name 'GCCVER'
                                    values '4.8', '5', '8'
                                }
                            }
                            exclude {
                                axis {
                                    name 'PLATFORM'
                                    values 'linux'
                                }
                                axis {
                                    name 'GCCVER'
                                    values '4.4.4', '6', '10'
                                }
                            }
                            exclude {
                                axis {
                                    name 'GCCVER'
                                    values '4.4.4'
                                }
                                axis {
                                    name 'STDVER'
                                    values '11', '17', '2x'
                                }
                            }
                            exclude {
                                axis {
                                    name 'GCCVER'
                                    values '4.8', '4.9', '5', '6', '7'
                                }
                                axis {
                                    name 'STDVER'
                                    values '17', '2x'
                                }
                            }
                            exclude {
                                axis {
                                    name 'GCCVER'
                                    values '8'
                                }
                                axis {
                                    name 'STDVER'
                                    values '2x'
                                }
                            }
                        }
                        stages {
                            stage('GCC Build and test') {
                                steps {
                                    sh """ echo "Building with GCC-${GCCVER} STD=${STD}${STDVER} WARN=${BUILD_WARNOPT} on ${PLATFORM}"
case "${PLATFORM}" in
            *openindiana*) BUILD_SSL_ONCE=true ;;
            *) BUILD_SSL_ONCE=false ;;
esac
export BUILD_SSL_ONCE

BUILD_TYPE=default-all-errors \
BUILD_WARNOPT="${BUILD_WARNOPT}" BUILD_WARNFATAL=yes \
CFLAGS="-std=${STD}${STDVER}" CXXFLAGS="-std=${STD}++${STDVER}" \
CC=gcc-${GCCVER} CXX=g++-${GCCVER} \
./ci_build.sh
"""
                                }
                            }
                        }
                    }
                } // stage for matrix BuildAndTest-GCC

                stage("BuildAndTest-CLANG") {
                    matrix {
                        agent { label "OS=${PLATFORM} && ${CLANGVER}" }
                        axes {
                            axis {
                                name 'PLATFORM'
                                values 'linux', 'openindiana'
                            }
                            axis {
                                name 'CLANGVER'
                                values '8', '9'
                            }
                            axis {
                                name 'BUILD_WARNOPT'
                                values 'minimal', 'medium', 'hard'
                            }
                            axis {
                                name 'STD'
                                values 'gnu' //, 'c'
                            }
                        }

                        excludes {
                            exclude {
                                axis {
                                    name 'PLATFORM'
                                    values 'linux'
                                }
                                axis {
                                    name 'CLANGVER'
                                    values '8', '9'
                                }
                            }
                        }
                        stages {
                            stage('CLANG Build and test') {
                                steps {
                                    sh """ echo "Building with CLANG-${CLANGVER} STD=${STD}${STDVER} WARN=${BUILD_WARNOPT} on ${PLATFORM}"
case "${PLATFORM}" in
            *openindiana*) BUILD_SSL_ONCE=true ;;
            *) BUILD_SSL_ONCE=false ;;
esac
export BUILD_SSL_ONCE

BUILD_TYPE=default-all-errors \
BUILD_WARNOPT="${BUILD_WARNOPT}" BUILD_WARNFATAL=yes \
CFLAGS="-std=${STD}${STDVER}" CXXFLAGS="-std=${STD}++${STDVER}" \
CC=clang-${CLANGVER} CXX=clang++-${CLANGVER} CPP=clang-cpp \
./ci_build.sh
"""
                                }
                            }
                        }
                    }
                } // stage for matrix BuildAndTest-CLANG

            } // parallel
        } // obligatory one stage
    } // obligatory stages
} // pipeline
