def HelperScript = null

pipeline {
    agent none

    parameters {
        booleanParam (
            name: 'DO_MATRIX_GCC',
            defaultValue: true,
            description: 'Check builds with GCC'
        )
        booleanParam (
            name: 'DO_MATRIX_CLANG',
            defaultValue: true,
            description: 'Check builds with CLANG'
        )
        booleanParam (
            name: 'DO_MATRIX_DISTCHECK',
            defaultValue: true,
            description: 'Check Makefile EXTRA_DIST and other nuances for usable dist archive creation'
        )
        booleanParam (
            name: 'DO_MATRIX_SHELL',
            defaultValue: true,
            description: 'Check shell script syntax'
        )
        booleanParam (
            name: 'DO_SPELLCHECK',
            defaultValue: true,
            description: 'Check spelling'
        )
    }

    options {
        skipDefaultCheckout()
    }

    stages {
        stage("Stash source for workers") {
/*
 * NOTE: For quicker builds, it is recommended to set up the pipeline job
 * using this Jenkinsfile to refer to a local copy of the NUT repository
 * maintained on the stashing worker (as a Reference Repo), and do just
 * shallow checkouts (depth=1). Longer history may make sense for release
 * builds with changelog generation, but not for quick test iterations.
 */
            agent { label "master-worker" }
            steps {
                /* clean up our workspace */
                deleteDir()
                /* clean up tmp directory */
                dir("${workspace}@tmp") {
                    deleteDir()
                }
                /* clean up script directory */
                dir("${workspace}@script") {
                    deleteDir()
                }
                checkout scm
                stash 'NUT-checkedout'

                // Offload code into external script to work around certain JVM limits
                script { HelperScript = load("Jenkinsfile-helper.groovy") }
            }
        }

        stage("pick BuildAndTest-GCC?") {
            when { expression { params.DO_MATRIX_GCC } }
            stages {
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
                    stage('Unstash SRC') {
                        steps {
                            script { HelperScript.unstashCleanNUTsrc() }
                        }
                    }
                    stage('GCC Build and test') {
                        steps {
                            script { HelperScript.doMatrixGCC("${GCCVER}", "${STD}", "${STDVER}", "${PLATFORM}", "${BUILD_WARNOPT}") }
                        }
                    }
                }
               }
              }
            }
        } // stage for matrix BuildAndTest-GCC

        stage("pick BuildAndTest-CLANG?") {
            when { expression { params.DO_MATRIX_CLANG } }
            stages {
             stage("BuildAndTest-CLANG") {
              matrix {
                agent { label "OS=${PLATFORM} && CLANGVER=${CLANGVER}" }
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
                        name 'STDVER'
                        values '99', '11', '17', '2x'
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
                    exclude {
                        axis {
                            name 'CLANGVER'
                            values '8'
                        }
                        axis {
                            name 'STDVER'
                            values '2x'
                        }
                    }
                }
                stages {
                    stage('Unstash SRC') {
                        steps {
                            script { HelperScript.unstashCleanNUTsrc() }
                        }
                    }
                    stage('CLANG Build and test') {
                        steps {
                            script { HelperScript.doMatrixCLANG("${CLANGVER}", "${STD}", "${STDVER}", "${PLATFORM}", "${BUILD_WARNOPT}") }
                        }
                    }
                }
               }
              }
            }
        } // stage for matrix BuildAndTest-CLANG

        stage('pick Shell-script checks?') {
            when { expression { params.DO_MATRIX_SHELL } }
            stages {
             stage("Shell-script checks") {
              matrix {
                agent { label "OS=${PLATFORM}" }
                axes {
                    axis {
                        name 'PLATFORM'
                        values 'linux', 'openindiana'
                    }
                    axis {
                        name 'SHELL_PROGS'
                        values 'bash', 'ksh', 'dash', 'ash', 'busybox_sh'
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
                            values 'ksh' // not all of my linux buildhosts have it
                        }
                    }
                    exclude {
                        axis {
                            name 'PLATFORM'
                            values 'openindiana'
                        }
                        axis {
                            name 'SHELL_PROGS'
                            values 'busybox_sh', 'ash'
                        }
                    }
                }
                stages {
                    stage('Unstash SRC') {
                        steps {
                            script { HelperScript.unstashCleanNUTsrc() }
                        }
                    }
                    stage('Shellcheck') {
                        steps {
                            warnError(message: 'Build-and-check step failed, proceeding to cover whole matrix') {
                                /* Note: currently `make check-scripts-syntax`
                                 * uses current system shell of the build/test
                                 * host, or bash where script says explicitly.
                                 * So currently SHELL_PROGS does not apply here.
                                 */
                                sh """ BUILD_TYPE=default-shellcheck ./ci_build.sh """
                            }
                        }
                    }
                    stage('NDE check') {
                        steps {
                            warnError(message: 'Build-and-check step failed, proceeding to cover whole matrix') {
                                sh """ BUILD_TYPE=nut-driver-enumerator-test SHELL_PROGS="${SHELL_PROGS}" ./ci_build.sh """
                            }
                        }
                    }
                }
               }
              }
            }
        } // stage for matrix Shell-script checks

        stage('pick Distchecks?') {
            when { expression { params.DO_MATRIX_DISTCHECK } }
            stages {
             stage("Distchecks") {
              matrix {
                agent { label "OS=${PLATFORM}" }
                axes {
                    axis {
                        name 'PLATFORM'
                        values 'linux', 'openindiana'
                    }
                    axis {
                        name 'BUILD_TYPE'
                        values 'default-tgt:distcheck-light', 'default-tgt:distcheck-valgrind'
                    }
                }
                stages {
                    stage('Unstash SRC') {
                        steps {
                            script { HelperScript.unstashCleanNUTsrc() }
                        }
                    }
                    stage('Test BUILD_TYPE') {
                        steps {
                            script { HelperScript.doMatrixDistcheck("${BUILD_TYPE}", "${PLATFORM}") }
                        }
                    }
                }
               }
              }
            }
        } // stage for matrix Distchecks

        stage('Unclassified tests') {
            parallel {

                stage('Spellcheck') {
                    when { expression { params.DO_SPELLCHECK } }
                    agent { label "OS=openindiana" }
                    stages {
                        stage('Unstash SRC') {
                            steps {
                                script { HelperScript.unstashCleanNUTsrc() }
                            }
                        }
                        stage('Check') {
                            steps {
                                warnError(message: 'Build-and-check step failed, proceeding to cover whole matrix') {
                                    sh """ BUILD_TYPE=default-spellcheck ./ci_build.sh """
                                }
                            }
                        }
                    }
                } // spellcheck

            } // parallel
        } // obligatory one stage
    } // obligatory stages

    post {
        always {
            script { HelperScript.doSummarizeIssues(env.JOB_NAME, env.BRANCH_NAME) }
        }
    }

} // pipeline

