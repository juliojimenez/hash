PORTNAME=	hash-shell
DISTVERSION=	11
CATEGORIES=	shells

MAINTAINER=	julio@julioj.com
COMMENT=	Modern command line interpreter with completion, aliases, and git integration
WWW=		https://github.com/juliojimenez/hash

LICENSE=	APACHE20
LICENSE_FILE=	${WRKSRC}/LICENSE

USE_GITHUB=	yes
GH_ACCOUNT=	juliojimenez
GH_PROJECT=	hash
GH_TAGNAME=	v${DISTVERSION}

PLIST_FILES=	bin/hash-shell

do-build:
	${MAKE_CMD} -C ${WRKSRC} CC=${CC} CFLAGS="${CFLAGS}"

do-install:
	${INSTALL_PROGRAM} ${WRKSRC}/hash-shell ${STAGEDIR}${PREFIX}/bin

.include <bsd.port.mk>
