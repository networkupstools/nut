#!/usr/bin/env groovy

// Separate the large blocks to stay inside the 64Kb Java method limit
// which includes the many iterations for the parallel/matrix code that
// is ultimately generated...
//   https://stackoverflow.com/questions/47628248/how-to-create-methods-in-jenkins-declarative-pipeline/47631522#47631522
//   https://code-held.com/2019/05/02/jenkins-pipeline-method-too-large/
//   https://issues.jenkins.io/browse/JENKINS-37984
//   https://issues.jenkins.io/browse/JENKINS-56500
//   https://support.cloudbees.com/hc/en-us/articles/360039361371-Method-Code-Too-Large-Error

// Collect reports from Warnings NG for each sub-build in this array:

// Per https://www.jenkins.io/doc/book/pipeline/shared-libraries/#defining-global-variables
// we need to annotate the following declaration to share that variable:
//@groovy.transform.Field
//def issueAnalysis = []

void doMatrixGCC(String GCCVER, String STD, String STDVER, String PLATFORM, String BUILD_WARNOPT) {
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
//        issueAnalysis << i
        publishIssues issues: [i], filters: [includePackage('io.jenkins.plugins.analysis.*')]
    }
} // doMatrixGCC()


void doMatrixCLANG(String CLANGVER, String STD, String STDVER, String PLATFORM, String BUILD_WARNOPT) {
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
//        issueAnalysis << i
        publishIssues issues: [i], filters: [includePackage('io.jenkins.plugins.analysis.*')]
    }
} // doMatrixCLANG()

void doMatrixDistcheck(String BUILD_TYPE, String PLATFORM) {
    warnError(message: 'Build-and-check step failed, proceeding to cover whole matrix') {
        sh """ BUILD_TYPE="${BUILD_TYPE}" ./ci_build.sh """
    }
    script {
        def id = "Distcheck:${BUILD_TYPE}@${PLATFORM}"
        def i = scanForIssues tool: gcc(name: id)
//        issueAnalysis << i
        //def i = scanForIssues tool: clang(name: id)
        //issueAnalysis << i
        publishIssues issues: [i], filters: [includePackage('io.jenkins.plugins.analysis.*')]
    }
} // doMatrixDistcheck()

void doSummarizeIssues(String JOB_NAME, String BRANCH_NAME) {
/*
    script {
        def reference = JOB_NAME.replace(BRANCH_NAME, "master")
        publishIssues id: 'analysis', name: 'All Issues',
            referenceJobName: reference,
            issues: issueAnalysis,
            filters: [includePackage('io.jenkins.plugins.analysis.*')]
    }
*/
}

/* Other repetitive code: */
void unstashCleanNUTsrc() {
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

//Important statement for loading the script!!!
return this
