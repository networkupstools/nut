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

                stage('Spellcheck') {
                    agent { label "OS=openindiana" }
                    steps {
                        sh """ BUILD_TYPE=default-spellcheck ./ci_build.sh """
                    }
                }

                stage('Shell-script checks') {
                    matrix {
                        agent { label "OS=platform" }
                        axes {
                            axis {
                                name 'PLATFORM'
                                values 'linux', 'openindiana'
                            }
                            axis {
                                name 'SHELL_PROGS'
                                values 'bash', 'ksh', 'zsh', 'dash', 'ash', 'busybox sh'
                            }
                        }
                        excludes {
                            exclude {
                                axis {
                                    name 'PLATFORM'
                                    values 'linux'
                                }
                                axis {
                                    name 'SHELL_PROGS'
                                    values 'ksh', 'zsh'
                                }
                            }
                            exclude {
                                axis {
                                    name 'PLATFORM'
                                    values 'openindiana'
                                }
                                axis {
                                    name 'SHELL_PROGS'
                                    values 'busybox sh', 'ash'
                                }
                            }
                        }
                        stages {
                            stage('Shellcheck') {
                                steps {
                                    sh """ BUILD_TYPE=default-shellcheck ./ci_build.sh """
                                }
                            }
                            stage('NDE check') {
                                steps {
                                    sh """ BUILD_TYPE=nut-driver-enumerator-test SHELL_PROGS="${SHELL_PROGS}" ./ci_build.sh """
                                }
                            }
                        }
                    }
                }

                stage('Distchecks') {
                    agent { label "OS=linux" }
                    matrix {
                        axes {
                            axis {
                                name 'BUILD_TYPE'
                                values 'default-tgt:distcheck-light', 'default-tgt:distcheck-valgrind'
                            }
                        }
                        stages {
                            stage('Test BUILD_TYPE') {
                                steps {
                                    sh """ BUILD_TYPE=default-distcheck-light ./ci_build.sh """
                                }
                            }
                        }
                    }
                }

            } // parallel
        } // obligatory one stage
    } // obligatory stages
} // pipeline
