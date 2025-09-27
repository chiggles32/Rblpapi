#include <Rcpp.h>
#include <blpapi_session.h>
#include <blpapi_service.h>
#include <blpapi_request.h>
#include <blpapi_event.h>
#include <blpapi_message.h>
#include <blpapi_element.h>
#include <blpapi_name.h>
using namespace Rcpp;

// [[Rcpp::export]]
SEXP _Rblpapi_bql(std::string query) {
    // Setup Bloomberg Session
    blpapi::SessionOptions sessionOptions;
    sessionOptions.setServerHost("localhost");
    sessionOptions.setServerPort(8194);
    blpapi::Session session(sessionOptions);

    if(!session.start()) {
        stop("Failed to start Bloomberg session");
    }
    if(!session.openService("//blp/bqlsvc")) {
        stop("Failed to open BQL service");
    }

    blpapi::Service service = session.getService("//blp/bqlsvc");
    blpapi::Request request = service.createRequest("sendQuery");
    request.set("expression", query.c_str());

    // Send the request and collect the response
    blpapi::CorrelationId cid;
    session.sendRequest(request, cid);

    std::vector<std::string> fields, ids;
    std::vector<double> values;

    bool done = false;
    while (!done) {
        blpapi::Event event = session.nextEvent();
        if (event.eventType() == blpapi::Event::RESPONSE ||
            event.eventType() == blpapi::Event::PARTIAL_RESPONSE) {
            blpapi::MessageIterator msgIter(event);
            while (msgIter.next()) {
                blpapi::Message msg = msgIter.message();
                if (strcmp(msg.messageType(), "BQLResponse") == 0) {
                    blpapi::Element results = msg.getElement("results");
                    for (size_t i = 0; i < results.numElements(); i++) {
                        blpapi::Element fieldElem = results.getElement(i);
                        std::string name = fieldElem.getElementAsString("name");
                        blpapi::Element idCol = fieldElem.getElement("idColumn");
                        blpapi::Element valCol = fieldElem.getElement("valuesColumn");
                        for (size_t j = 0; j < idCol.numValues(); j++) {
                            fields.push_back(name);
                            ids.push_back(idCol.getValueAsString(j));
                            values.push_back(valCol.getValueAsFloat64(j));
                        }
                    }
                }
            }
            if (event.eventType() == blpapi::Event::RESPONSE)
                done = true;
        }
    }

    // Return a data.frame as R list
    return DataFrame::create(
        Named("field") = fields,
        Named("id") = ids,
        Named("value") = values
    );
}
