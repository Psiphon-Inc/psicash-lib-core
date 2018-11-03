/*
 * Copyright (c) 2018, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef PSICASHLIB_HTTP_STATUS_CODES_H
#define PSICASHLIB_HTTP_STATUS_CODES_H

namespace psicash {

// Adapted from Golang's net/http/status.go

constexpr const unsigned int kHTTPStatusContinue           = 100; // RFC 7231, 6.2.1
constexpr const unsigned int kHTTPStatusSwitchingProtocols = 101; // RFC 7231, 6.2.2
constexpr const unsigned int kHTTPStatusProcessing         = 102; // RFC 2518, 10.1

constexpr const unsigned int kHTTPStatusOK                   = 200; // RFC 7231, 6.3.1
constexpr const unsigned int kHTTPStatusCreated              = 201; // RFC 7231, 6.3.2
constexpr const unsigned int kHTTPStatusAccepted             = 202; // RFC 7231, 6.3.3
constexpr const unsigned int kHTTPStatusNonAuthoritativeInfo = 203; // RFC 7231, 6.3.4
constexpr const unsigned int kHTTPStatusNoContent            = 204; // RFC 7231, 6.3.5
constexpr const unsigned int kHTTPStatusResetContent         = 205; // RFC 7231, 6.3.6
constexpr const unsigned int kHTTPStatusPartialContent       = 206; // RFC 7233, 4.1
constexpr const unsigned int kHTTPStatusMultiStatus          = 207; // RFC 4918, 11.1
constexpr const unsigned int kHTTPStatusAlreadyReported      = 208; // RFC 5842, 7.1
constexpr const unsigned int kHTTPStatusIMUsed               = 226; // RFC 3229, 10.4.1

constexpr const unsigned int kHTTPStatusMultipleChoices   = 300; // RFC 7231, 6.4.1
constexpr const unsigned int kHTTPStatusMovedPermanently  = 301; // RFC 7231, 6.4.2
constexpr const unsigned int kHTTPStatusFound             = 302; // RFC 7231, 6.4.3
constexpr const unsigned int kHTTPStatusSeeOther          = 303; // RFC 7231, 6.4.4
constexpr const unsigned int kHTTPStatusNotModified       = 304; // RFC 7232, 4.1
constexpr const unsigned int kHTTPStatusUseProxy          = 305; // RFC 7231, 6.4.5
constexpr const unsigned int kHTTPStatus306               = 306; // RFC 7231, 6.4.6 (Unused)
constexpr const unsigned int kHTTPStatusTemporaryRedirect = 307; // RFC 7231, 6.4.7
constexpr const unsigned int kHTTPStatusPermanentRedirect = 308; // RFC 7538, 3

constexpr const unsigned int kHTTPStatusBadRequest                   = 400; // RFC 7231, 6.5.1
constexpr const unsigned int kHTTPStatusUnauthorized                 = 401; // RFC 7235, 3.1
constexpr const unsigned int kHTTPStatusPaymentRequired              = 402; // RFC 7231, 6.5.2
constexpr const unsigned int kHTTPStatusForbidden                    = 403; // RFC 7231, 6.5.3
constexpr const unsigned int kHTTPStatusNotFound                     = 404; // RFC 7231, 6.5.4
constexpr const unsigned int kHTTPStatusMethodNotAllowed             = 405; // RFC 7231, 6.5.5
constexpr const unsigned int kHTTPStatusNotAcceptable                = 406; // RFC 7231, 6.5.6
constexpr const unsigned int kHTTPStatusProxyAuthRequired            = 407; // RFC 7235, 3.2
constexpr const unsigned int kHTTPStatusRequestTimeout               = 408; // RFC 7231, 6.5.7
constexpr const unsigned int kHTTPStatusConflict                     = 409; // RFC 7231, 6.5.8
constexpr const unsigned int kHTTPStatusGone                         = 410; // RFC 7231, 6.5.9
constexpr const unsigned int kHTTPStatusLengthRequired               = 411; // RFC 7231, 6.5.10
constexpr const unsigned int kHTTPStatusPreconditionFailed           = 412; // RFC 7232, 4.2
constexpr const unsigned int kHTTPStatusRequestEntityTooLarge        = 413; // RFC 7231, 6.5.11
constexpr const unsigned int kHTTPStatusRequestURITooLong            = 414; // RFC 7231, 6.5.12
constexpr const unsigned int kHTTPStatusUnsupportedMediaType         = 415; // RFC 7231, 6.5.13
constexpr const unsigned int kHTTPStatusRequestedRangeNotSatisfiable = 416; // RFC 7233, 4.4
constexpr const unsigned int kHTTPStatusExpectationFailed            = 417; // RFC 7231, 6.5.14
constexpr const unsigned int kHTTPStatusTeapot                       = 418; // RFC 7168, 2.3.3
constexpr const unsigned int kHTTPStatusUnprocessableEntity          = 422; // RFC 4918, 11.2
constexpr const unsigned int kHTTPStatusLocked                       = 423; // RFC 4918, 11.3
constexpr const unsigned int kHTTPStatusFailedDependency             = 424; // RFC 4918, 11.4
constexpr const unsigned int kHTTPStatusUpgradeRequired              = 426; // RFC 7231, 6.5.15
constexpr const unsigned int kHTTPStatusPreconditionRequired         = 428; // RFC 6585, 3
constexpr const unsigned int kHTTPStatusTooManyRequests              = 429; // RFC 6585, 4
constexpr const unsigned int kHTTPStatusRequestHeaderFieldsTooLarge  = 431; // RFC 6585, 5
constexpr const unsigned int kHTTPStatusUnavailableForLegalReasons   = 451; // RFC 7725, 3

constexpr const unsigned int kHTTPStatusInternalServerError           = 500; // RFC 7231, 6.6.1
constexpr const unsigned int kHTTPStatusNotImplemented                = 501; // RFC 7231, 6.6.2
constexpr const unsigned int kHTTPStatusBadGateway                    = 502; // RFC 7231, 6.6.3
constexpr const unsigned int kHTTPStatusServiceUnavailable            = 503; // RFC 7231, 6.6.4
constexpr const unsigned int kHTTPStatusGatewayTimeout                = 504; // RFC 7231, 6.6.5
constexpr const unsigned int kHTTPStatusHTTPVersionNotSupported       = 505; // RFC 7231, 6.6.6
constexpr const unsigned int kHTTPStatusVariantAlsoNegotiates         = 506; // RFC 2295, 8.1
constexpr const unsigned int kHTTPStatusInsufficientStorage           = 507; // RFC 4918, 11.5
constexpr const unsigned int kHTTPStatusLoopDetected                  = 508; // RFC 5842, 7.2
constexpr const unsigned int kHTTPStatusNotExtended                   = 510; // RFC 2774, 7
constexpr const unsigned int kHTTPStatusNetworkAuthenticationRequired = 511; // RFC 6585, 6

} // namespace psicash

#endif //PSICASHLIB_HTTP_STATUS_CODES_H
