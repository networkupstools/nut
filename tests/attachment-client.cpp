/* Small NIT client for the attachment protocol and its public API aliases. */
#include "common.h"
#include "../clients/nutclient.h"
#include "../clients/upsclient.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
	if (argc != 3) {
		return 2;
	}

	const std::string mode(argv[1]);
	const uint16_t port = static_cast<uint16_t>(std::atoi(argv[2]));
	try {
		if (mode == "c-detach" || mode == "c-detach-tls") {
			UPSCONN_t connection;
			memset(&connection, 0, sizeof(connection));
			if (mode == "c-detach-tls") {
				(void)upscli_init(0, NULL, NULL, NULL);
			}
			if (upscli_connect(&connection, "127.0.0.1", port,
				mode == "c-detach-tls" ? UPSCLI_CONN_REQSSL : 0) < 0) {
				return 1;
			}
			return upscli_disconnect(&connection);
		}
		if (mode == "c-query-count" || mode == "c-query-lower-count"
		||  mode == "c-query-legacy-count"
		) {
			UPSCONN_t connection;
			memset(&connection, 0, sizeof(connection));
			if (upscli_connect(&connection, "127.0.0.1", port, 0) < 0) {
				return 1;
			}
			const char *query[] = {mode == "c-query-legacy-count" ? "NUMLOGINS"
				: (mode == "c-query-lower-count" ? "numattach" : "NUMATTACH"), "dummy"};
			size_t count;
			char **answer;
			const int result = upscli_get(&connection, 2, query, &count, &answer);
			if (result < 0) {
				std::cout << upscli_upserror(&connection) << std::endl;
			} else {
				std::cout << answer[2] << std::endl;
			}
			upscli_disconnect(&connection);
			return result < 0 ? 1 : 0;
		}
		if (mode == "c-api-count" || mode == "c-api-attach" || mode == "c-api-detach") {
			NUTCLIENT_t client = nutclient_tcp_create_client("127.0.0.1", port);
			if (!client) {
				return 1;
			}
			if (mode == "c-api-attach") {
				nutclient_device_attach(client, "dummy");
			} else if (mode == "c-api-detach") {
				nutclient_detach(client);
			} else {
				std::cout << nutclient_get_device_num_attach(client, "dummy") << std::endl;
			}
			nutclient_destroy(client);
			return 0;
		}

		nut::TcpClient client;
		client.connect("127.0.0.1", port, false);
		/* Exercise the base API too: no new virtual slot is required. */
		nut::Client& base = client;
		if (mode == "attach") {
			base.deviceAttach("dummy");
		} else if (mode == "login") {
			base.deviceLogin("dummy");
		} else if (mode == "detach") {
			base.detach();
		} else if (mode == "logout") {
			base.logout();
		} else if (mode == "count") {
			std::cout << base.deviceGetNumAttach("dummy") << std::endl;
		} else if (mode == "numlogins") {
			std::cout << base.deviceGetNumLogins("dummy") << std::endl;
		} else {
			return 2;
		}
	} catch (const nut::NutException& error) {
		std::cerr << error.what() << std::endl;
		return 1;
	}
	return 0;
}
