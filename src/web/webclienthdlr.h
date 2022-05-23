#ifndef WEBCLIENTHANDLER_H
#define WEBCLIENTHANDLER_H

#include <mdz_proto_http/httpv1_server.h>
#include <mutex>

struct webClientParams
{
    webClientParams()
    {
    }

    std::string softwareVersion;
    std::string httpDocumentRootDir;
};

class WebClientHdlr : public Mantids::Network::HTTP::HTTPv1_Server
{
public:
    WebClientHdlr(void *parent, Streamable *sock);
    ~WebClientHdlr() override;
    void setWebClientParameters(const webClientParams &newWebClientParameters);

protected:
    /**
     * @brief processClientRequest Process web client request
     * @return http responce code.
     */
    Mantids::Network::HTTP::Response::StatusCode processClientRequest() override;

private:

    std::string commonName;
    webClientParams webClientParameters;


};



#endif // WEBCLIENTHANDLER_H
