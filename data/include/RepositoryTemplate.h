#pragma once

template<typename Query, typename Result>
class RepositoryTemplate {
public:
    Result request(const Query& query) const
    {
        onBeforeRequest(query);
        validate(query);
        Result result = doRequest(query);
        onAfterRequest(query, result);
        return result;
    }

    virtual ~RepositoryTemplate() = default;

protected:
    virtual void onBeforeRequest(const Query&) const {}
    virtual void validate(const Query&) const {}
    virtual Result doRequest(const Query& query) const = 0;
    virtual void onAfterRequest(const Query&, Result&) const {}
};
