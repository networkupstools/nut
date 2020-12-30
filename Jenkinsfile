def issueAnalysis = []

pipeline {
    agent none
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
            agent { label "jimoi" }
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
            }
        }

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
                            unstashCleanNUTsrc()
                        }
                    }
                    stage('GCC Build and test') {
                        steps {
                            doMatrixGCC("${GCCVER}", "${STD}", "${STDVER}", "${PLATFORM}", "${BUILD_WARNOPT}")
                        }
                    }
                }
            }
        } // stage for matrix BuildAndTest-GCC

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
                            unstashCleanNUTsrc()
                        }
                    }
                    stage('CLANG Build and test') {
                        steps {
                            doMatrixCLANG("${CLANGVER}", "${STD}", "${STDVER}", "${PLATFORM}", "${BUILD_WARNOPT}")
                        }
                    }
                }
            }
        } // stage for matrix BuildAndTest-CLANG

        stage('Shell-script checks') {
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
                            unstashCleanNUTsrc()
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
        } // stage for matrix Shell-script checks

        stage('Distchecks') {
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
                            unstashCleanNUTsrc()
                        }
                    }
                    stage('Test BUILD_TYPE') {
                        steps {
                            warnError(message: 'Build-and-check step failed, proceeding to cover whole matrix') {
                                sh """ BUILD_TYPE="${BUILD_TYPE}" ./ci_build.sh """
                            }
                            script {
                                def id = "Distcheck:${BUILD_TYPE}@${PLATFORM}"
                                def i = scanForIssues tool: gcc(name: id)
                                issueAnalysis << i
                                //def i = scanForIssues tool: clang(name: id)
                                //issueAnalysis << i
                                publishIssues issues: [i], filters: [includePackage('io.jenkins.plugins.analysis.*')]
                            }
                        }
                    }
                }
            }
        } // stage for matrix Distchecks

        stage('Unclassified tests') {
            parallel {

                stage('Spellcheck') {
                    agent { label "OS=openindiana" }
                    stages {
                        stage('Unstash SRC') {
                            steps {
                                unstashCleanNUTsrc()
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
            script {
                def reference = env.JOB_NAME.replace(env.BRANCH_NAME, "develop")
                publishIssues id: 'analysis', name: 'All Issues',
                    referenceJobName: reference,
                    issues: issueAnalysis,
                    filters: [includePackage('io.jenkins.plugins.analysis.*')]
            }
        }
    }

} // pipeline


// Separate the large blocks to stay inside the 64Kb Java method limit
// which includes the many iterations for the parallel/matrix code that
// is ultimately generated...
//   https://stackoverflow.com/questions/47628248/how-to-create-methods-in-jenkins-declarative-pipeline/47631522#47631522
//   https://code-held.com/2019/05/02/jenkins-pipeline-method-too-large/
//   https://issues.jenkins.io/browse/JENKINS-37984
//   https://issues.jenkins.io/browse/JENKINS-56500
//   https://support.cloudbees.com/hc/en-us/articles/360039361371-Method-Code-Too-Large-Error
def doMatrixGCC(String GCCVER, String STD, String STDVER, String PLATFORM, String BUILD_WARNOPT) {
    warnError(message: 'Build-and-check step failed, proceeding to cover whole matrix') {
        sh """ echo "Building with GCC-${GCCVER} STD=${STD}${STDVER} WARN=${BUILD_WARNOPT} on ${PLATFORM}"
case "${PLATFORM}" in
    *openindiana*) BUILD_SSL_ONCE=true ; BUILD_LIBGD_CGI=auto ; export BUILD_LIBGD_CGI ;;
    *) BUILD_SSL_ONCE=false ;;
esac
export BUILD_SSL_ONCE

case "${STDVER}" in
    99) STDXXVER="98" ;;
    *) STDXXVER="${STDVER}" ;;
esac

BUILD_TYPE=default-all-errors \
BUILD_WARNOPT="${BUILD_WARNOPT}" BUILD_WARNFATAL=yes \
CFLAGS="-std=${STD}${STDVER}" CXXFLAGS="-std=${STD}++\${STDXXVER}" \
CC=gcc-${GCCVER} CXX=g++-${GCCVER} \
./ci_build.sh
"""
    } // warnError + sh

    script {
        def id = "GCC-${GCCVER}:STD=${STD}${STDVER}:WARN=${BUILD_WARNOPT}@${PLATFORM}"
        def i = scanForIssues tool: gcc(name: id)
        issueAnalysis << i
        publishIssues issues: [i], filters: [includePackage('io.jenkins.plugins.analysis.*')]
    }
} // doMatrixGCC()


def doMatrixCLANG(String CLANGVER, String STD, String STDVER, String PLATFORM, String BUILD_WARNOPT) {
    warnError(message: 'Build-and-check step failed, proceeding to cover whole matrix') {
        sh """ echo "Building with CLANG-${CLANGVER} STD=${STD}${STDVER} WARN=${BUILD_WARNOPT} on ${PLATFORM}"
case "${PLATFORM}" in
    *openindiana*) BUILD_SSL_ONCE=true ; BUILD_LIBGD_CGI=auto ; export BUILD_LIBGD_CGI ;;
    *) BUILD_SSL_ONCE=false ;;
esac
export BUILD_SSL_ONCE

case "${STDVER}" in
    99) STDXXVER="98" ;;
    *) STDXXVER="${STDVER}" ;;
esac

BUILD_TYPE=default-all-errors \
BUILD_WARNOPT="${BUILD_WARNOPT}" BUILD_WARNFATAL=yes \
CFLAGS="-std=${STD}${STDVER}" CXXFLAGS="-std=${STD}++\${STDXXVER}" \
CC=clang-${CLANGVER} CXX=clang++-${CLANGVER} CPP=clang-cpp \
./ci_build.sh
"""
    }
    script {
        def id = "CLANG-${CLANGVER}:STD=${STD}${STDVER}:WARN=${BUILD_WARNOPT}@${PLATFORM}"
        def i = scanForIssues tool: clang(name: id)
        issueAnalysis << i
        publishIssues issues: [i], filters: [includePackage('io.jenkins.plugins.analysis.*')]
    }
} // doMatrixCLANG()

def unstashCleanNUTsrc() {
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
    unstash 'NUT-checkedout'
} // unstashCleanNUTsrc()

